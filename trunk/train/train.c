/**
* CS 452: User Task
* becmacdo
* dgoc
*/
#define DEBUG 2

#include <string.h>
#include <ts7200.h>
#include <buffer.h>
#include <math.h>

#include "debug.h"
#include "error.h"
#include "requests.h"
#include "routeplanner.h"
#include "servers.h"
#include "trackserver.h"
#include "train.h"
#include "ui.h"
#include "shell.h"

#define	SPEED_HIST				10
#define	HEARTBEAT_MS			200
#define	HEARTBEAT_TICKS			(HEARTBEAT_MS /MS_IN_TICK)
#define	TIME_UPDATE				-1
#define WATCHMAN_PRIORITY		3
#define CALIBRATION_DEST		37 // E11 
#define INIT_DEST				28 // D9
#define LOCATE_TIMEOUT			(5000 /MS_IN_TICK)
#define LOCATE_TIMEOUT_INC		(2000 /MS_IN_TICK)
#define	INIT_GRACE				(10000 /MS_IN_TICK)
#define	SENSOR_TIMEOUT			(5000 / MS_IN_TICK)
#define LOCATE_GEAR				4
#define	CAL_GEAR				10
#define SD_THRESHOLD			10	// parts of mean	
#define CAL_LOOPS				2
#define	C						4000
#define	INT_MAX					0x7fffffff
// Private Stuff
// ----------------------------------------------------------------------------

typedef struct {
	int 		mm;
	int 		ms;
} Speed;

typedef struct {
	int  		id;				// Identifying number
	int			defaultGear;	// Default speed setting

	int  		sensor;			// Last known location
	int			trigger;		// last time we know a sensor

	int  		dest;			// Desired End Location
	TrainMode	mode;			// Train Mode (calibration or normal)
		
	int 	    gear;			// The current speed setting
	bool		gearChanged;	// Has the speed changed
	int			lastStopTime;	// Time of last stop
	Speed		velocity;		// The actual current speed (in mm/s)
	Speed		baseVel;		// The last measured speed
	int			stopDist;
	
	TID 		rpTid;			// Id of the Route Planner
	TID 		tsTid;			// Id of the Track Server
	TID			deTid;			// Id of the Detective
	TID			csTid;			// Id of the Clock Server
	TID			uiTid;			// Id of the UI server
	TID			htTid;			// Id of the Heart
	TID			coTid;			// Id of the courier

	int			cal_loops;
	int			sd;				// Standard deviation
	Speed		histBuf[SPEED_HIST];
	RB			hist;	
} Train;

inline int 
speed( Speed sp ) { // in mm/s
	if( sp.ms == 0 ) return INT_MAX;
	return (sp.mm * 1000) / sp.ms;
}

inline int	
speed_time( Speed sp, int mm ) { // in ms
	if( sp.mm == 0 ) return INT_MAX;
	assert( sp.mm != 0 );
	return (sp.ms * mm) / sp.mm;
}

inline int 
speed_dist( Speed sp, int ms ) { // in mm
	assert (sp.ms != 0 );
	return (sp.mm * ms) / sp.ms;
}

inline Speed 
speed_adjust( Speed sp, int original, int final ) {
	sp.mm *= final;
	sp.ms *= original;
	return sp;
}

inline Speed 
speed_add( Speed sp1, Speed sp2 ) {
	Speed sp;
	sp.ms = (sp1.ms * sp2.ms) / 1000;
	sp.mm = ((sp1.mm * sp2.ms)/ 1000) + ((sp2.mm * sp1.ms) / 1000);
	debug( "speed_add: %dmm/s + %dmm/s = %d/%d = %d mm/s\r\n",
			speed( sp1 ), speed( sp2 ), sp.mm, sp.ms, speed( sp ) );
	return sp;
}


void 		train_init    		( Train *tr );
Speed 		train_speed			( Train *tr );
void 		train_adjustSpeed	( Train *tr, int mm );
void 		train_calibrate		( Train *tr );

// Route Planner Commands
RPReply		train_planRoute 	( Train *tr );
int 		train_distance		( Train *tr, int sensor ); // in mm

// Detective Commands
void 		train_locate		( Train *tr, int timeout );
void 		train_detect		( Train *tr, SensorsPred *pred, int mm );
void 		train_update		( Train *tr, int sensor, int ticks );

// Track Server Commands
void 		train_reverse		( Train *tr );
void 		train_drive			( Train *tr, int gear );
void		train_flipSwitches	( Train *tr, RPReply *rpReply );
SwitchDir	train_dir			( Train *tr, int sw );

		
// UI Server
void train_updateUI( Train *tr, int mm );

// Watchman
void 		train_watch 		( Train *tr, RPReply *rep );
void		heart				();
void		courier				();
// ----------------------------------------------------------------------------

void train_run () {

	Train 	 	tr;
	RPReply   	rpReply;		// Reply from the Route Planner
	TrainReq	req;
	int 		senderTid, len, distTraveled = 0;
	int			timeout = LOCATE_TIMEOUT;

	// Initialize this train
	train_init ( &tr );

	FOREVER {
		// Receive a server request
		len = Receive ( &senderTid, &req, sizeof(req) );
	
		assert( len == sizeof(req) );
		
		switch (req.type) {
			case TIME_UPDATE: // heartbeat
				assert( senderTid == tr.htTid );
				assert( tr.mode == NORMAL );
				if( tr.mode == NORMAL ) {
					Reply( senderTid, 0, 0 );
					// Update current location
					distTraveled -= speed_dist( tr.velocity, HEARTBEAT_MS );
					// Send this information to the UI
					train_updateUI( &tr, distTraveled );
					// Adjust speed
					train_adjustSpeed( &tr, rpReply.stopDist - distTraveled );
				}
				// Resend detective request
				train_detect( &tr, &rpReply.nextSensors, distTraveled );
				break;

			case WATCH_TIMEOUT:		// train is lost
				Reply( senderTid, 0, 0 );
				printf( "Train is lost, for %dms.\r\n", timeout * MS_IN_TICK );
				train_locate( &tr, timeout );
				break;

			case STRAY_TIMEOUT:		// train is really lost
				Reply( senderTid, 0, 0 );
				timeout += (2000 / MS_IN_TICK); // increase the timeout by 2s 
				printf( "Train is really lost for %dms.\r\n", timeout *MS_IN_TICK );
				train_reverse( &tr );
				train_locate( &tr, timeout );
				break;

			case POS_UPDATE: 	// got a position update
				Reply( senderTid, 0, 0 );
				debug ( "train: #%d is at sensor %c%d\r\n",
					tr.id, sensor_bank( req.sensor ), sensor_num( req.sensor ) );
				// Update the train's information
				train_update( &tr, req.sensor, req.ticks );
				// Ask the Route Planner for an efficient route!
				rpReply = train_planRoute ( &tr );
				//debug( "train: has route stopDist=%d\r\n", rpReply.stopDist );
				if( rpReply.err < NO_ERROR ) {
					error( rpReply.err,  "Route Planner failed." );
					assert( 1 == 0 );
				}

				if( rpReply.stopDist < 0 ) {
					train_reverse( &tr );			// TODO this
					rpReply.stopDist = -rpReply.stopDist;
				}
				// Reset the distance counter
				distTraveled = 0;

				// Flip the switches along the path.
				train_flipSwitches( &tr, &rpReply );

				// Adjust the speed according to distance
				train_adjustSpeed( &tr, rpReply.stopDist );

				// Resend detective request
				train_detect( &tr, &rpReply.nextSensors, distTraveled );

				// Start off the heart task
				if( tr.mode == NORMAL ) { Reply ( tr.htTid, 0, 0 ); }
				break;

			case DEST_UPDATE: 	// got a new destination to go to
				Reply( senderTid, 0, 0 );
				debug ( "train: #%d is at sensor %d going to destidx %d\r\n",
							tr.id, tr.sensor, req.dest);
				// Update the internal dest variable
				tr.dest = req.dest;
				tr.mode = req.mode;
				break;

			case STOP_UPDATE:
				Reply( senderTid, 0, 0 );
				// Store the stopping distance
				debug( "train: stopping distance %dmm stored.\r\n", req.mm );
				tr.stopDist = req.mm;
				break;

			default:
				//error
				break;
	

		}
	}
}

void train_init ( Train *tr ) {

	TID tid;
	TrainReq init;
	
	// Get the train number from shell
	Receive ( &tid, &init, sizeof(TrainReq) );
	assert( init.type == TRAIN_INIT );
	
	// Copy the data to the train
	tr->id   		= init.id;
	tr->defaultGear = init.gear;

	// Reply to the shell
	Reply( tid, &tr->id, sizeof(int) );
	// The train talks to several servers
	tr->rpTid = WhoIs (ROUTEPLANNER_NAME);
	tr->tsTid = WhoIs (TRACK_SERVER_NAME);
	tr->deTid = WhoIs (DETECTIVE_NAME);
	tr->csTid = WhoIs (CLOCK_NAME);
	tr->uiTid = WhoIs (UI_NAME);
	tr->htTid = Create( TRAIN_PRTY - 1, &heart );
	tr->coTid = Create( TRAIN_PRTY - 1, &courier );
	assert( tr->rpTid >= NO_ERROR );
	assert( tr->tsTid >= NO_ERROR );
	assert( tr->deTid >= NO_ERROR );
	assert( tr->csTid >= NO_ERROR );
	assert( tr->uiTid >= NO_ERROR );
	assert( tr->htTid >= NO_ERROR );
	assert( tr->coTid >= NO_ERROR );

	// Block the heart
	Receive( &tid, &init, sizeof(TrainReq) ); 
	assert( tid == tr->htTid );

	// Initialize internal variables
	tr->mode = INIT_LOC;
	tr->sensor = 0;
	tr->gear = tr->defaultGear;

	// Initialize the calibration data
	rb_init( &(tr->hist), tr->histBuf );
	memoryset( tr->histBuf, 0, sizeof(tr->histBuf) );
	Speed sp = {350, 1000}; // inital speed
	rb_push( &(tr->hist), &sp );


	tr->velocity.mm = 0;
	tr->velocity.ms = 0;
	tr->trigger = Time( tr->csTid );
	tr->gearChanged = false;
	tr->cal_loops = 0;
	tr->stopDist = 0;


//	RegisterAs ("Train");;
}

Speed train_speed( Train *tr ) {
	Speed total = {0, 0}, *rec;
	int mean, var = 0, tmp;

	// Calculate the mean
	foreach( rec, tr->histBuf ) {
		total.mm += rec->mm;
		total.ms += rec->ms;
		//debug("buffer %d/%d = \t%dmm/s\r\n", rec->mm, rec->ms, speed( rec ));
	}

	mean = speed(total);

	// Calculate standard deviation
	foreach( rec, tr->histBuf ) {
		tmp = abs( speed( *rec ) - mean );
		var += tmp * tmp;
	}

	tr->sd = isqrt( var );

	// Exit calibration mode once we have good enough data
	if( tr->mode == CAL_SPEED 
			&& (tr->sd < ( mean / SD_THRESHOLD ) 
				|| tr->cal_loops >= CAL_LOOPS ) ) {
		printf( "Train: finished calibration with sd=%d\r\n", tr->sd );
		tr->mode = CAL_STOP;
	}

	return total;
}


void train_adjustSpeed( Train *tr, int mm ) {
	int gear = tr->defaultGear;
	
	if( tr->mode == NORMAL ) {
		// TODO: PLACE speed adjusting here
		if( mm <= 0 ) { 
			debug( "train: thinks it's reached its destination." );
			gear = 1;
			//gear = 0;
		} else 	if( mm < 600 ) {
			gear = 3;
		}
	}
	train_drive( tr, gear);
	assert( tr->gear == gear );

	Speed base = train_speed( tr );
	// Adjust calibrated speed based on gear
	tr->velocity = speed_adjust( base, tr->defaultGear, tr->gear );
	debug( "train: speed adjusted to %dmm/s\r\n", speed( tr->velocity ) );
}


void train_update( Train *tr, int sensor, int ticks ) {
	int 		last, pred, diff;
	Speed		sp;

	// Calculate the distance traveled
	sp.ms = ( ticks - tr->trigger ) * MS_IN_TICK;
	sp.mm = train_distance( tr, sensor );
	last = speed( sp );
	
	// Calculate the predicted speed
	pred = speed( tr->velocity );
	
	// Update the mean speed
	if(  tr->mode == CAL_SPEED ) {
		debug( "adding speed %d to history data\r\n");
		rb_force( &tr->hist, &sp );
	}
	tr->gearChanged = false;

	// See how far we were off
	diff = speed_dist( tr->velocity, sp.ms ) - sp.mm;

	if( abs(diff) > 100 ) 		{ printf("\033[31m"); }
	else if( abs(diff) > 50 ) 	{ printf("\033[33m"); }
	else if( abs(diff) > 20 ) 	{ printf("\033[36m"); }
	else 						{ printf("\033[32m"); }
	printf("%d/%d = \t%dmm/s \tmu:%dmm/s \tsd:%dmm/s: \tdiff:%dmm\033[37m\r\n",
			sp.mm, sp.ms, last, pred, tr->sd, diff);

	// TODO remove following lines
	//time = (Time( tr->csTid ) - tr->lastStopTime) * MS_IN_TICK; 
	// calculate acceleration
	//printf("train: acceleration is %dmm/s^2\r\n", (last * 1000)/ time );

	
	tr->sensor = sensor; 
	tr->trigger = ticks;
}

//-----------------------------------------------------------------------------
//-------------------------- Route Planner Commands ---------------------------
//-----------------------------------------------------------------------------

int train_distance( Train *tr, int sensor ) {
	RPRequest	req;
	int	dist;
	
	req.type		= MIN_SENSOR_DIST;
	req.sensor1		= tr->sensor;
	req.sensor2		= sensor;

	Send( tr->rpTid, &req,  sizeof(RPRequest), &dist, sizeof(int) );
	

	//debug( "train: distance returned %d \r\n", dist );
	return dist;
}


RPReply train_planRoute( Train *tr ) {
	RPReply 	reply;
	RPRequest	req;

	// Initialize the Route Planner request
	req.type		= PLANROUTE;
	req.trainId		= tr->id;
	req.lastSensor	= tr->sensor;
	req.destIdx		= tr->dest;
	req.avgSpeed	= min( speed( tr->velocity ), 0 );

	Send( tr->rpTid, &req, sizeof(RPRequest), &reply, sizeof(RPReply) );
	
	return reply;
}

//-----------------------------------------------------------------------------
//-------------------------- Detective Commands -------------------------------
//-----------------------------------------------------------------------------

void train_locate( Train *tr, int timeout ) {
	DeRequest	req;
	req.type = GET_STRAY;
	req.expire = timeout + Time( tr->csTid );
	
	// Slow down to locate speed
	train_drive( tr, LOCATE_GEAR );

	// Get stray sensors
	Send( tr->coTid, &req, sizeof(DeRequest), 0, 0 );
}

void train_retract( Train *tr ) {
	DeReply 	rpl;
	DeRequest	req;
	req.type	= RETRACT;
	req.tid  	= tr->coTid;

	// Retract the courier
	Send( tr->deTid, &req, sizeof(DeRequest), &rpl, sizeof(TSReply) );

	// Receive the retracted msg - we can't because the heart might be sending
//	Receive( &tid, &req, sizeof(DeRequest) );
}

void train_detect( Train *tr, SensorsPred *pred, int mm ) {
	DeRequest	req;
	int 		i, dist, ticks = Time( tr->csTid );
	int			variation = max( tr->sd, speed( tr->velocity )/ SD_THRESHOLD );

	Speed		window = { variation * 3 , 1000 };
	Speed		upper = speed_add( tr->velocity, window );
	window.mm *= -1;
	Speed		lower = speed_add( tr->velocity, window );

	lower		= speed_adjust( lower, tr->defaultGear, tr->gear );
	upper		= speed_adjust( upper, tr->defaultGear, tr->gear );

	assert( pred->len <= array_size( req.events ) );
	req.type      = WATCH_FOR;
	req.numEvents = pred->len;
	req.expire 	  = 0;

//	if( rep.stopAction == STOP_AND_REVERSE ) 

	debug("predicting sensors:");

	for( i = 0; i < req.numEvents ; i ++ ) {
		dist = pred->dists[i] - mm ;

		debug("%c%d, ", sensor_bank(pred->idxs[i]), sensor_num(pred->idxs[i]));

		req.events[i].sensor = pred->idxs[i];
		req.events[i].start  = ticks + speed_time( lower, dist );
		req.events[i].end    = ticks + speed_time( upper, dist );

		req.expire = max( req.expire, req.events[i].end );
	}
	debug( "\r\n" );
	//if( req.expire > tr->trigger + SENSOR_TIMEOUT ) { }
	debug( "train: req.expire=%dms in future\r\n", (req.expire - ticks) * MS_IN_TICK );
	debug( "train: pr:%dmm/s \twi:%dmm/s \tlo:%dmm/s \tup:%dmm/s\r\n", 
		speed( tr->velocity ), speed(window ), speed( lower ), speed( upper ) );
	
	// Tell the detective about the Route Planner's prediction.
	Send( tr->coTid, &req,	sizeof(DeRequest), 0 , 0 );
}

//-----------------------------------------------------------------------------
//-------------------------- Track Server Commands ----------------------------
//-----------------------------------------------------------------------------
void train_reverse( Train *tr ) {
	TSRequest req;
	TSReply	  reply;
	
	if( tr->mode == CAL_SPEED || tr->mode == CAL_STOP ) return;
	
	debug ("\033[44mTrain %d is reversing at gear =%d\033[49m\r\n", tr->id, tr->gear );

	tr->gearChanged = true;

	req.type = RV;
	req.train = tr->id;
	req.startTicks	= Time(tr->csTid);

	Send( tr->tsTid, &req, sizeof(TSRequest), &reply, sizeof(TSReply) );
	tr->lastStopTime == Time( tr->csTid );
}

void train_drive( Train *tr, int gear ) {
	TSRequest req;
	TSReply	  reply;

	if( tr->mode == CAL_SPEED ) return;
	
	if( tr->gear == gear ) return;

	tr->gearChanged = true;
	debug ("Train %d is driving at gear =%d\r\n", tr->id, gear);

	if( gear >= 0 && gear < 15 ) {
		tr->gear = gear;
	}
		
	req.type = TR;
	req.train = tr->id;
	req.speed = gear;
	req.startTicks	= Time(tr->csTid);

	Send( tr->tsTid, &req, sizeof(TSRequest), &reply, sizeof(TSReply) );


	if( gear == 0 ) tr->lastStopTime == Time( tr->csTid );
}

void train_flipSwitches( Train *tr, RPReply *rpReply ) {
	TSRequest		req;
	TSReply			reply;
	SwitchSetting  *ss;

	if( tr->mode != NORMAL && tr->mode != INIT_LOC ) return;

	foreach( ss, rpReply->switches ) {
	  if( ss->id <= 0 ) break;

	  	// only flip switches in the inproper position

	  	if ( train_dir( tr, ss->id ) != ss->dir ) {
		//	debug("train: flip sw%d to %c\r\n", ss->id, switch_dir(ss->dir));
			req.type = SW;
			req.sw   = ss->id;
			req.dir  = ss->dir;
			req.startTicks	= Time(tr->csTid);

			Send( tr->tsTid, &req, sizeof(TSRequest), &reply, sizeof(TSReply) );
		}
	}
}

SwitchDir train_dir( Train *tr, int sw ) {
	TSRequest 	req = { ST, {sw}, {0} };
	TSReply		reply;
	
	req.startTicks	= Time(tr->csTid);

	Send( tr->tsTid, &req, sizeof(TSRequest), &reply, sizeof(TSReply) );
	return reply.dir;
}

		
//-----------------------------------------------------------------------------
//--------------------------------- UI Server ---------------------------------
//-----------------------------------------------------------------------------

void train_updateUI( Train *tr, int mm ) { 
	UIRequest 	req;
	int 		tmp;
	
	// Send this information to the UI
	req.type = TRAIN;
	req.idx  = tr->sensor;
	req.dist = mm;
	Send( tr->uiTid, &req, sizeof(UIRequest), &tmp, sizeof(int) ); 
}

void courier() {
	debug( "courier starting\r\n");
	TID 		deTid = WhoIs (DETECTIVE_NAME);
	TID			trTid;
	DeRequest 	req;
	DeReply		rpl;
	TrainReq	ans;

	assert( deTid >= NO_ERROR );

	FOREVER {
		debug( "courier receiving\r\n");
		// Reveice a detective request
		Receive	( &trTid, &req, sizeof( DeRequest ) );
		Reply	(  trTid, 0, 0 );

		// Ask the detective
		Send	(  deTid, &req, sizeof( DeRequest ), &rpl, sizeof( DeReply ) );

		if( rpl.sensor >= NO_ERROR ) {
			ans.type 	= POS_UPDATE;
		} else if( rpl.sensor == TIMEOUT && req.type == GET_STRAY ) {
			ans.type 	= STRAY_TIMEOUT;
		} else if( rpl.sensor == TIMEOUT && req.type == WATCH_FOR ) {
			ans.type	= WATCH_TIMEOUT;
		} else {
			ans.type 	= rpl.sensor;
		}

		ans.sensor 	= rpl.sensor;
		ans.ticks 	= rpl.ticks;
		
		// Give the result back to the train
		Send	(  trTid, &ans, sizeof( TrainReq ), 0, 0 );
	}
}


void heart() {
	TID parent	= MyParentTid();
	TID csTid 	= WhoIs( CLOCK_NAME );
	TrainReq	req;

	req.type  = TIME_UPDATE;
	FOREVER {
		Send( parent, &req, sizeof(TrainReq), 0, 0 );
		// Heart beat for the watchman
		req.ticks = Delay( HEARTBEAT_TICKS , csTid );
	}
}

