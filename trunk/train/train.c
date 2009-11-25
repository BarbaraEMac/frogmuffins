/**
* CS 452: User Task
* becmacdo
* dgoc
*/
#define DEBUG 1

#include <string.h>
#include <ts7200.h>
#include <buffer.h>
#include <math.h>

#include "debug.h"
#include "error.h"
#include "globals.h"
#include "requests.h"
#include "routeplanner.h"
#include "servers.h"
#include "shell.h"
#include "trackserver.h"
#include "train.h"
#include "ui.h"

#define	SPEED_HIST				10
#define	HEARTBEAT_MS			200
#define	HEARTBEAT_TICKS			(HEARTBEAT_MS /MS_IN_TICK)
#define CALIBRATION_DEST		37 // E11 
#define INIT_DEST				28 // D9
#define LOCATE_TIMEOUT			(5000 /MS_IN_TICK)
#define LOCATE_TIMEOUT_INC		(2000 /MS_IN_TICK)
#define	INIT_GRACE				(10000 /MS_IN_TICK)
#define	SENSOR_TIMEOUT			(5000 / MS_IN_TICK)
#define LOCATE_GEAR				4
#define	CAL_GEAR				10
#define SD_THRESHOLD			10	// parts of mean	
#define CAL_LOOPS				1
#define INT_MAX					0x7FFFFFFF
#define CALIBRATE_GEAR			7
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
	TID			caTid;			// ID of the calibration task
	TID			rwTid;			// ID of the route watch dog task
	TID			shTid;			// ID of the shell

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
	//debug( "speed_add: %dmm/s + %dmm/s = %d/%d = %d mm/s\r\n",
	//		speed( sp1 ), speed( sp2 ), sp.mm, sp.ms, speed( sp ) );
	return sp;
}

void 		train_init    		( Train *tr );
Speed 		train_avgSpeed		( Train *tr );
void 		train_adjustSpeed	( Train *tr, int mm );
int 		train_getStopDist	( Train *tr, int gear );

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
void 		calibration			();
void 		routeWatchDog 		();
//----------------------------------------------------------------------------

void train_run () {

	Train 	 	tr;
	RPReply   	rpReply;		// Reply from the Route Planner
	TrainReq	req;
	int 		senderTid, len, distTraveled = 0;
	int			timeout = LOCATE_TIMEOUT;

	// Initialize this train
	train_init ( &tr );

	// The train should figure out where it is
	train_locate( &tr, timeout );
	
	FOREVER {
		// Receive a server request
		len = Receive ( &senderTid, &req, sizeof(req) );
		assert( len == sizeof(req) );

		printf ("receiving a request from %d of type %d\r\n", senderTid, req.type );
		
		switch (req.type) {

			case TIME_UPDATE: 	// heartbeat
				assert( senderTid == tr.htTid );
				assert( tr.mode == NORMAL );
				
				if( tr.mode == NORMAL ) {
					printf ("TIME UPDATE REPLYING TO %d\r\n", senderTid);
					Reply( senderTid, 0, 0 );
					// Update current location (NOTE: This is an estimate!)
					distTraveled += speed_dist( tr.velocity, HEARTBEAT_MS );
					// Send this information to the UI
					train_updateUI( &tr, distTraveled );
					// Adjust speed
					train_adjustSpeed( &tr, rpReply.stopDist - distTraveled );
				}
				
				// Resend detective request
				train_detect( &tr, &rpReply.nextSensors, distTraveled );
				break;

			case WATCH_TIMEOUT:		// train is lost
				printf ("WATCH TIMEOUT REPLUYING TO %d\r\n", senderTid);
				Reply( senderTid, 0, 0 );
				printf( "Train is lost, for %dms.\r\n", timeout * MS_IN_TICK );
				
				// Try to hit a sensor to find where you are
				train_locate( &tr, timeout );
				break;

			case STRAY_TIMEOUT:		// train is really lost
				printf ("STRAY TIMEOUT REPLUYING TO %d\r\n", senderTid);
				Reply( senderTid, 0, 0 );
				
				// Increase the timeout by 2s 
				timeout += (2000 / MS_IN_TICK); 
				printf( "Train is really lost for %dms.\r\n", timeout *MS_IN_TICK );
				
				// Reverse direction 
				train_reverse( &tr );
				// Try to hit a sensor to find where you are
				train_locate( &tr, timeout );
				break;
			
			case DEST_UPDATE: 		// got a new destination to go to
			//	debug ( "train: #%d is at sensor %d going to destidx %d\r\n",
			//				tr.id, tr.sensor, req.dest);
				printf ("Got a DEST UPDATE for dest=%d mode=%d\r\n", req.dest, req.mode);
				
				if( req.mode == NORMAL ) { 
					printf ("DEST UPDATE REPLYING TO %d\r\n", senderTid);
					// If not calibration, reply right away
					Reply( senderTid, 0, 0 );
				} else {
					// Don't reply until we have the info we need.
					assert( senderTid == tr.caTid );
				}

				// Update the internal dest variable
				tr.dest = req.dest;
				tr.mode = req.mode;

				// Set up the request to mimic a position update
				req.sensor = tr.sensor;
				req.ticks  = Time( tr.csTid );

				//HACk
				tr.gearChanged = true;

			case POS_UPDATE: 		// got a position update
				debug ( "train: POS UP #%d is at sensor %c%d. Going to %d\r\n",
					tr.id, sensor_bank( req.sensor ), sensor_num( req.sensor ), tr.dest );
				
				// Do not reply twice for a DEST_UPDATE
				if ( req.type == POS_UPDATE ) {
					printf ("REPLYING IN POS_UPDATE to %d\r\n", senderTid);
					Reply( senderTid, 0, 0 );
				}
					
				// Reset the distance counter
				distTraveled = 0;

				// Update the train's information
				train_update( &tr, req.sensor, req.ticks );
				
				// Ask the Route Planner for an efficient route!
				rpReply = train_planRoute ( &tr );

				// If there is no path available,
				if ( rpReply.err == NO_PATH ) {
					debug ("The train has no route. Stopping.\r\n");
					// Stop moving
					train_drive( &tr, 0 );

					// Wake up the route watcher so that we can try again later
					Send( tr.rwTid, &req.sensor, sizeof(int), 0, 0 );
					// Stop executing this code.
					break;

				// If there is another error, then we can't handle it.
				} else if( rpReply.err < NO_ERROR ) {
					error( rpReply.err,  "Route Planner failed." );
					assert( 1 == 0 );
				}
				
				// Reverse if the train is backwards
				if( rpReply.stopDist < 0 ) {
					debug ("Train is backwards. It's reversing.\r\n");
					train_reverse( &tr );			// TODO this
					rpReply.stopDist = -rpReply.stopDist;
				}
				printf( "train: has route stopDist=%d to %d\r\n", rpReply.stopDist, tr.dest );

				// Adjust the speed according to distance
				train_adjustSpeed( &tr, rpReply.stopDist );

				if ( rpReply.stopDist == 0 ) {
					if ( tr.mode == CAL_SPEED ) {
						// Reply to the Calibration task
						printf("SPEED: telling the calibrator we've reached a dest.\r\n");
						Reply( tr.caTid, &tr.sd, sizeof(int) );
					}
					if ( tr.mode == CAL_STOP ) {
						printf ("CAL_STOP SENDING TO SHELL\r\n");
						// Reply to the shell
						Reply( tr.shTid, 0, 0 );
					}
					break;
				}

				// Flip the switches along the path.
				train_flipSwitches( &tr, &rpReply );

				// Resend detective request
				train_detect( &tr, &rpReply.nextSensors, distTraveled );

				// Start off the heart task
				if( tr.mode == NORMAL ) { 
					printf ("REPLYING TO THE HEART\r\n");
					Reply( tr.htTid, 0, 0 ); 
				}
				
				break;

			case STOP_UPDATE:
				printf( "train: stopping distance %dmm stored.\r\n", req.mm );
				printf ("STOP UPDATE REPLYING TO %d\r\n", senderTid );
				Reply( senderTid, 0, 0 );
				
				// Store the stopping distance
				tr.stopDist = req.mm;
				
				break;

			default:
				//error
				break;
		}
	}
}

void train_init ( Train *tr ) {
	TrainReq init;
	TID		 tid;
	
	// Get the train number from shell
	Receive ( &tr->shTid, &init, sizeof(TrainReq) );
	assert( init.type == TRAIN_INIT );
	
	// Reply to the shell
	//Reply( tr->shTid, &tr->id, sizeof(int) );
	
	// Copy the data to the train
	tr->id   		= init.id;
	tr->defaultGear = init.gear;

	// The train talks to several servers
	tr->rpTid = WhoIs (ROUTEPLANNER_NAME);
	tr->tsTid = WhoIs (TRACK_SERVER_NAME);
	tr->deTid = WhoIs (DETECTIVE_NAME);
	tr->csTid = WhoIs (CLOCK_NAME);
	tr->uiTid = WhoIs (UI_NAME);
	tr->htTid = Create( TRAIN_PRTY - 1, &heart );
	printf ("HEART = %d\r\n", tr->htTid);
	tr->caTid = Create( TRAIN_PRTY - 1, &calibration );
	printf ("CALI = %d\r\n", tr->caTid);
	tr->rwTid = Create( TRAIN_PRTY - 1, &routeWatchDog );
	printf ("RouteWatch = %d\r\n", tr->rwTid);
	assert( tr->rpTid >= NO_ERROR );
	assert( tr->tsTid >= NO_ERROR );
	assert( tr->deTid >= NO_ERROR );
	assert( tr->csTid >= NO_ERROR );
	assert( tr->uiTid >= NO_ERROR );
	assert( tr->htTid >= NO_ERROR );
	assert( tr->caTid >= NO_ERROR );

	// Block the heart
	Receive( &tid, &init, sizeof(TrainReq) ); 
	assert( tid == tr->htTid );

	// Initialize internal variables
	tr->mode   = CAL_SPEED;
	tr->sensor = 0; // A1
	tr->gear   = 0;

	// Initialize the calibration data
	rb_init( &(tr->hist), tr->histBuf );
	memoryset( tr->histBuf, 0, sizeof(tr->histBuf) );
	Speed sp = {350, 1000}; // inital speed
	rb_push( &(tr->hist), &sp );

	tr->velocity = train_avgSpeed (tr);
	tr->trigger = Time( tr->csTid );
	tr->gearChanged = false;
	tr->cal_loops = 0;
	tr->stopDist = 0;


//	RegisterAs ("Train");;
}

Speed train_avgSpeed( Train *tr ) {
	Speed total = {0, 0}, *rec;
	int   mean, var = 0, tmp;

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
	if ( tr->mode == CAL_SPEED && 
		(tr->sd < ( mean / SD_THRESHOLD ) || tr->cal_loops >= CAL_LOOPS) ) {
		printf( "Train: finished calibration with sd=%d\r\n", tr->sd );
	}

	return total;
}

void train_adjustSpeed( Train *tr, int distLeft ) {
	int bestGear = tr->defaultGear;
	int stopDist = 0;
	int i;
	
	// If we are not in calibration mode,
	if( tr->mode == NORMAL ) {
		
		if( distLeft <= 50 ) { 
			printf( "train: thinks it's reached its destination." );
			bestGear = 0;
		} 
		
		// Given the stop distance for this train,
		// estimate the best gear for the train to travel at.
		// In theory, as distLeft decreases, so does the speed.
		for ( i = 14; i >= 1; i -- ) {
			stopDist = train_getStopDist( tr, i );

			// Multiply by 2 to account for acc/deceleration
			if ( (stopDist*2) <= distLeft ) {
				bestGear = i;
				break;
			}
		}
	} 

	if ( tr->mode == CAL_STOP && distLeft == 0 ) {
		printf ("STOPPPPPPPPPPPPPPING\r\n");
		bestGear = 0;
	}
	// Change your speed to this new gear.
	train_drive( tr, bestGear );

	Speed base = train_avgSpeed( tr );
	
	// Adjust calibrated speed based on gear
	tr->velocity = speed_adjust( base, tr->defaultGear, tr->gear );
	debug( "train: speed adjusted to %dmm/s\r\n", speed( tr->velocity ) );
}

// HARDCODING
// Returns the stop distance in mm given the gear and train id
int train_getStopDist( Train *tr, int gear ) {
	switch( gear ) {
		case 14:
		case 13:
			return 830;
		case 12:
		case 11:
			return 820;
		case 10:
		case 9:
			return 600;
		case 8:
		case 7:
			return 490;
		case 6:
		case 5:
			return 330;
		case 4:
		case 3:
			return 210;
		case 2:
		case 1:
			return 100;
		default:
			return 500;
	}
}

void train_update( Train *tr, int sensor, int ticks ) {
	int 		last, pred, diff;
	Speed		sp;

	sp.ms = ( ticks - tr->trigger ) * MS_IN_TICK;
	sp.mm = train_distance( tr, sensor );
	last = speed( sp );
	
	// Calculate the predicted speed
	pred = speed( tr->velocity );
	
	// Update the mean speed
	if(  tr->gearChanged == false ) { //TODO: this is wrong
		printf ( "adding speed %d/%d to history data\r\n", sp.mm, sp.ms );
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
	
	tr->sensor  = sensor; 
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
	DeRequest req;
	req.type   = GET_STRAY;
	req.expire = timeout + Time( tr->csTid );
	
	// Drive at locate speed
	train_drive( tr, LOCATE_GEAR );

	// Get stray sensors
	Send( tr->deTid, &req, sizeof(DeRequest), 0, 0 );
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
//	debug( "train: req.expire=%dms in future\r\n", (req.expire - ticks) * MS_IN_TICK );
//	debug( "train: pr:%dmm/s \twi:%dmm/s \tlo:%dmm/s \tup:%dmm/s\r\n", 
//		speed( tr->velocity ), speed(window ), speed( lower ), speed( upper ) );
	
	// Tell the detective about the Route Planner's prediction.
	Send( tr->deTid, &req,	sizeof(DeRequest), 0 , 0 );
}

//-----------------------------------------------------------------------------
//-------------------------- Track Server Commands ----------------------------
//-----------------------------------------------------------------------------
void train_reverse( Train *tr ) {
	TSRequest req;
	TSReply	  reply;

	// Do not reverse if we are testing the stopping distance.
	if( tr->mode == CAL_STOP ) return; //|| tr->mode == CAL_SPEED ) return;
	
	debug( "\033[44mTrain %d is reversing at gear =%d\033[49m\r\n", 
			tr->id, tr->gear );

	tr->gearChanged = true;

	req.type        = RV;
	req.train       = tr->id;
	req.startTicks	= Time(tr->csTid);

	Send( tr->tsTid, &req, sizeof(TSRequest), &reply, sizeof(TSReply) );
	tr->lastStopTime == Time( tr->csTid );
}

void train_drive( Train *tr, int gear ) {
	TSRequest req;
	TSReply	  reply;

	// Don't change your speed while calibrating
	if( tr->mode == CAL_SPEED && tr->gear != CALIBRATE_GEAR ) {
		debug ("drivign at the cal speed\r\n");
		gear = CALIBRATE_GEAR;
	} else if ( tr->mode == CAL_SPEED ) return;
	
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

	// Do not change the switches if we are calibrating the speed
	if ( tr->mode == CAL_STOP ) return;

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

//-----------------------------------------------------------------------------
//----------------------------------- Heart -----------------------------------
//-----------------------------------------------------------------------------
void heart() {
	TID parent	= MyParentTid();
	TID csTid 	= WhoIs( CLOCK_NAME );
	TrainReq	req;

	req.type  = TIME_UPDATE;
	FOREVER {
		Send( parent, &req, sizeof(TrainReq), 0, 0 );
		
		// Heart beat for the train
		req.ticks = Delay( HEARTBEAT_TICKS , csTid );
	}
}

//-----------------------------------------------------------------------------
//--------------------------------- Calibration -------------------------------
//-----------------------------------------------------------------------------
void calibration () {
	int dests[] = { 37, 23, 15, 1, 22, 27 };
	int i, j, sd;
	TrainReq req;
	TID 	 parent = MyParentTid();

	
	// Find D9
	printf ("CALI: Finding D9\r\n");
	req.type = DEST_UPDATE;
	req.dest = INIT_DEST;		// D9
	req.mode = CAL_SPEED;	
	Send( parent, &req, sizeof(TrainReq), &sd, sizeof(int) );
	
	// Change your mode to calibrate the speed
	
	for ( i = 0; i < CAL_LOOPS; i ++ ) {
		for ( j = 0; j < 6; j ++ ) {
			printf ("CALIBRATE: Train finding %d\r\n", dests[j]);
			req.dest = dests[j];

			Send( parent, &req, sizeof(TrainReq), &sd, sizeof(int) );
			if ( sd < SD_THRESHOLD ) {
				break;
			}
		}
			
		if ( sd < SD_THRESHOLD ) {
//			printf ("Train is done calibration with sd=%d.\r\n", sd);
			break;
		}
	}

	req.dest = 37;				// E11
	req.mode = CAL_STOP;		
	printf ("CALIBRATE: Finding E11\r\n");
	Send( parent, &req, sizeof(TrainReq), &sd, sizeof(int) );
	printf ("			CALIBRATION TASK IS EXITING\r\n");
/*
	// Go park at A5
	req.dest = 2;				// A5
	req.mode = NORMAL;		
	printf ("CALIBRATE: Parking at A5\r\n");
	Send( parent, &req, sizeof(TrainReq), &sd, sizeof(int) );
*/

	Exit();
}

//-----------------------------------------------------------------------------
//------------------------------ Route Watch Dog ------------------------------
//-----------------------------------------------------------------------------
void routeWatchDog () {
	TrainReq req;
	int 	 parent = MyParentTid();
	int 	 trainTid;
	int		 clockTid = WhoIs( CLOCK_NAME );
	int 	 lastHitSensor;

	req.type = POS_UPDATE;

	FOREVER {
		// Wait until the path is blocked
		Receive( &trainTid, &lastHitSensor, sizeof(int) );
		assert ( trainTid == parent );
		Reply  ( trainTid, &req, sizeof(TrainReq) );
		
		debug ("routeWatchDog: Waking up the train. There may be a path.\r\n");

		// Wait a bit
		req.ticks = Delay( 500 / MS_IN_TICK, clockTid );

		// Wake up the train with a fake position update
		req.sensor = lastHitSensor;
		Send( trainTid, &req, sizeof(TrainReq), 0, 0 );
	}

	Exit(); // This task will never reach here
}
