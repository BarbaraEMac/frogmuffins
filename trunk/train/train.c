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
#define SD_THRESHOLD			10	// parts of mean	
#define CAL_LOOPS				1
#define INT_MAX					0x7FFFFFFF
#define LOCATE_GEAR				4
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
	Speed		velocity;		// The actual current speed (in mm/s)
	int			stopDist;		// The measured stopping distance		
	
	TID 		rpTid;			// Id of the Route Planner
	TID 		tsTid;			// Id of the Track Server
	TID			csTid;			// Id of the Clock Server
	TID			uiTid;			// Id of the UI server
	TID			caTid;			// ID of the calibration task
	//TID			coTid;			// ID of the courier task
	TID			rwTid;			// ID of the route watch dog task
	TID			shTid;			// ID of the shell

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
void 		train_adjustSpeed	( Train *tr, int distLeft, enum StopAction action );
int 		train_getStopDist	( Train *tr, int gear );

// Route Planner Commands
RPReply		train_planRoute 	( Train *tr );
int 		train_distance		( Train *tr, int sensor ); // in mm

// Detective Commands
void 		train_locate		( Train *tr, int timeout, DeRequest *predct);
void 		train_predictSensors( Train *tr, SensorsPred *sensor, int mm, DeRequest *predct );
void 		train_updatePos		( Train *tr, int sensor, int ticks );

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
void 		calibration			();
void 		routeWatchDog 		();
//----------------------------------------------------------------------------

void train_run () {

	Train 	 	tr;
	RPReply   	rpReply;		// Reply from the Route Planner
	TrainReq	req;
	DeRequest	predct;
	int 		senderTid, len, distFromSensor = 0;
	int			timeout = LOCATE_TIMEOUT;
	bool		routeWatcherAwake = false;

	// Initialize this train
	train_init ( &tr );

	// The train should figure out where it is
	train_locate( &tr, timeout, &predct );
	
	FOREVER {
		// Receive a server request
		len = Receive ( &senderTid, &req, sizeof(req) );
		assert( len == sizeof(req) );

		switch (req.type) {

			case TIME_UPDATE:		// heartbeat
				if( tr.mode == DRIVE || tr.mode == CAL_STOP ) {
					// Update current location (NOTE: This is an estimate!)
					distFromSensor += speed_dist( tr.velocity, HEARTBEAT_MS );
					// Send this information to the UI
					train_updateUI( &tr, distFromSensor );
					// Adjust speed
					train_adjustSpeed( &tr, rpReply.stopDist - distFromSensor, rpReply.stopAction );
					// Only update predictions if train is not lost 
					if( predct.type == WATCH_FOR ) {
						// Resend detective request
						train_predictSensors( &tr, &rpReply.nextSensors, 
								distFromSensor, &predct );
					}
				}
				Reply( senderTid, &predct, sizeof(DeRequest) );
				break;

			case WATCH_TIMEOUT:		// train is lost
				Reply( senderTid, 0, 0 );
				
				if ( tr.mode != IDLE ) {
					printf( "Train is lost, for %dms.\r\n", timeout * MS_IN_TICK );
					
					// Try to hit a sensor to find where you are
					train_locate( &tr, timeout, &predct );
				}
				break;

			case STRAY_TIMEOUT:		// train is really lost
				Reply( senderTid, 0, 0 );
				
				if ( tr.mode != IDLE ) {
					// Increase the timeout by 2s 
					timeout += (2000 / MS_IN_TICK); 
					printf( "Train is really lost for %dms.\r\n", timeout *MS_IN_TICK );
					
					// Reverse direction 
					train_reverse( &tr );
					// Try to hit a sensor to find where you are
					train_locate( &tr, timeout, &predct );
				}
				break;
			
			case DEST_UPDATE: 		// got a new destination to go to
			//	debug ( "train: #%d is at sensor %d going to destidx %d\r\n",
			//				tr.id, tr.sensor, req);
			//	printf ("Got a DEST UPDATE for dest=%d mode=%d\r\n", req.dest, req.mode);
				
				if( req.mode == DRIVE ) { 
					debug ("drive mode replying to %d\r\n", senderTid );
					// If not calibration, reply right away
					Reply( senderTid, 0, 0 );
				}
			//	else {
					// Don't reply until we have the info we need.
			//		assert( senderTid == tr.caTid );
			//	}
			//
			//

				// Make sure the route watcher is not blocked on the train.
				if ( routeWatcherAwake == true ) {
					routeWatcherAwake = false;
					Reply( tr.rwTid, 0, 0 );
				}

				// Update the internal dest variable
				tr.dest 		 = req.dest;
				tr.mode		 	 = req.mode;

				// Set up the request to mimic a position update
				req.sensor = tr.sensor;

				// TODO:
				if ( tr.mode != CAL_SPEED ) {
					req.ticks  = Time( tr.csTid );
				}

			case POS_UPDATE: 		// got a position update
//				printf ( "train: POS UP #%d is at sensor %c%d. Going to %d\r\n",
//					tr.id, sensor_bank( req.sensor ), sensor_num( req.sensor ), tr.dest );
				
				// Do not reply twice for a DEST_UPDATE
				if ( req.type == POS_UPDATE ) {
					Reply( senderTid, 0, 0 );
					
					// Update the train's information
					train_updatePos( &tr, req.sensor, req.ticks );
				}
					
				// Reset the distance counter
				distFromSensor = 0;
				
				// Ask the Route Planner for an efficient route!
				rpReply = train_planRoute ( &tr );

				// If there is no path available,
				if ( rpReply.err <= NO_PATH ) {
					debug ("The train has no route. Stopping.\r\n");
					
					assert ( rpReply.stopDist == 0 );

					printf("train: waking up routewatcher.\r\n");
					// Wake up the route watcher so that we can try again later
					// Non-Blocking
					routeWatcherAwake = true;
					Send( tr.rwTid, &req.sensor, sizeof(int), 0, 0 );
				
				// If there is another error, then we can't handle it.
			//	} else if( rpReply.err < NO_ERROR ) {
			//		error( rpReply.err,  "Route Planner failed." );
			//		assert( 1 == 0 );
				}
				
				// Reverse if the train is backwards
				if( rpReply.stopDist < 0 ) {
					debug ("Train is backwards. It's reversing.\r\n");
					train_reverse( &tr );			// TODO: Timeout incorrect for this mode.
					rpReply.stopDist = -rpReply.stopDist;
				}
				debug( "train: has route stopDist=%d to %d\r\n", rpReply.stopDist, tr.dest );

				// Adjust the speed according to distance
				train_adjustSpeed( &tr, rpReply.stopDist, rpReply.stopAction );

				// If we have reached our destination, 
				 if ( rpReply.stopDist == 0 ) {
					if ( tr.mode == CAL_SPEED ) {
						debug ("replying to the calibrator\r\n");
						// Reply to the Calibration task
						Reply( tr.caTid, 0, 0 );
					} else if ( tr.mode == CAL_STOP ) {
						debug ("cal stopped. replying to the shell.\r\n");
						tr.mode = IDLE; // HACk
						// Reply to the shell
						Reply( tr.shTid, 0, 0 );
					}
				// Otherwise, continue along your path towards the destination.
				} else {

					// Flip the switches along the path.
					train_flipSwitches( &tr, &rpReply );
					
					// Update the detective request
					train_predictSensors( &tr, &rpReply.nextSensors,
							distFromSensor, &predct );
		
				}
				
				break;

			case STOP_UPDATE:
				// Store the measured stopping distance for the default gear
				printf( "Train: Stopping distance %dmm calculated.\r\n", req.mm );
				tr.stopDist = req.mm;
				
				Reply( senderTid, 0, 0 );
				break;

			default:
				//error
				break;
		}
	}
}

void train_init ( Train *tr ) {
	TrainReq init;
	
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
	//tr->deTid = WhoIs (DETECTIVE_NAME);
	tr->csTid = WhoIs (CLOCK_NAME);
	tr->uiTid = WhoIs (UI_NAME);
	Create( TRAIN_PRTY - 1, &heart );
	//tr->coTid = Create( TRAIN_PRTY - 1, &courier );
	tr->caTid = Create( TRAIN_PRTY - 1, &calibration );
	tr->rwTid = Create( TRAIN_PRTY - 1, &routeWatchDog );
	assert( tr->rpTid >= NO_ERROR );
	assert( tr->tsTid >= NO_ERROR );
	//assert( tr->deTid >= NO_ERROR );
	assert( tr->csTid >= NO_ERROR );
	assert( tr->uiTid >= NO_ERROR );
	assert( tr->caTid >= NO_ERROR );
//	assert( tr->coTid >= NO_ERROR );

	// Initialize internal variables
	tr->mode   = CAL_SPEED;
	tr->sensor = 0; // A1
	tr->gear   = 0;

	// Initialize the calibration data
	rb_init( &(tr->hist), tr->histBuf );
	memoryset( tr->histBuf, 0, sizeof(tr->histBuf) );
	Speed sp = {350, 1000}; // inital speed
	rb_push( &(tr->hist), &sp );

	tr->velocity     = train_avgSpeed (tr);
	tr->trigger      = Time( tr->csTid );
	tr->gearChanged  = false;
	tr->stopDist     = 0;
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
/*
	// Exit calibration mode once we have good enough data
	if ( tr->mode == CAL_SPEED && (tr->sd < ( mean / SD_THRESHOLD ) ) ) {
		printf( "Train: finished calibration with sd=%d\r\n", tr->sd );
	}
*/
	return total;
}

void train_adjustSpeed( Train *tr, int distLeft, enum StopAction action ) {
	int bestGear = tr->defaultGear;
	int stopDist = 0;
	int defaultStopDist = train_getStopDist( tr, tr->defaultGear );
	int i;
	
	// If we are not in calibration mode,
	if( tr->mode == DRIVE ) {
		// Given the stop distance for this train,
		// estimate the best gear for the train to travel at.
		// In theory, as distLeft decreases, so does the speed.
		for ( i = tr->defaultGear; i >= 1; i -- ) {
			stopDist = train_getStopDist( tr, i );

			// Use the stopping distance multiplier to better predict
			stopDist *= tr->stopDist;
			stopDist /= defaultStopDist;

			// Multiply by  to account for acc/deceleration
			if ( (stopDist*3) <= distLeft ) {
				bestGear = i;
				break;
			}
		}

		if ( action == STOP_AND_REVERSE && bestGear < 3 ) {
			bestGear = 3;
		}

		// If we need to stop,
		if ( distLeft == 0 ) {
			bestGear = 0;
			tr->mode = IDLE;
		}
		
//		printf ("picking %d as best gear for %d left.\r\n", bestGear, distLeft);
	} 

	// If you are stopping calibration,
	if ( tr->mode == CAL_STOP && distLeft == 0 ) {
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
		case 0:
			return 0;
	}
}

void train_updatePos( Train *tr, int sensor, int ticks ) {
	int 		last, pred, diff;
	Speed		sp;

	sp.ms = ( ticks - tr->trigger ) * MS_IN_TICK;
	sp.mm = train_distance( tr, sensor );
	last  = speed( sp );
	
	// Calculate the predicted speed
	pred = speed( tr->velocity );
	
	// Update the mean speed
	if( tr->gearChanged == false && tr->gear == tr->defaultGear ) { //TODO: this is wrong
//		printf ( "adding speed %d/%d to history data\r\n", sp.mm, sp.ms );
		
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

//	printf("train: asking for distance.\r\n");
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

//	printf("train: asking for route.\r\n");
	Send( tr->rpTid, &req, sizeof(RPRequest), &reply, sizeof(RPReply) );
	
	return reply;
}

//-----------------------------------------------------------------------------
//-------------------------- Detective Commands -------------------------------
//-----------------------------------------------------------------------------
void train_locate( Train *tr, int timeout, DeRequest *predct ) {
	predct->type	= GET_STRAY;
	predct->expire	= timeout + Time( tr->csTid );
	predct->tid 	= MyTid();//tr->coTid; // TODO save this somewhere
	
	// Drive at locate speed
	train_drive( tr, LOCATE_GEAR );
}

void train_predictSensors( Train *tr, SensorsPred *sensor, int mm, DeRequest * predct ) {
	int 		i, dist, ticks = Time( tr->csTid );
	int			variation = max( tr->sd, speed( tr->velocity )/ SD_THRESHOLD );

	Speed		window = { variation * 3 , 1000 };
	Speed		upper = speed_add( tr->velocity, window );
	window.mm *= -1;
	Speed		lower = speed_add( tr->velocity, window );

	lower		= speed_adjust( lower, tr->defaultGear, tr->gear );
	upper		= speed_adjust( upper, tr->defaultGear, tr->gear );

	assert( sensor->len <= array_size( predct->events ) );
	
	predct->type 		= WATCH_FOR;
	predct->numEvents	= sensor->len;
	predct->expire 		= 0;
	predct->tid 		= MyTid(); //tr->coTid; TODO save this somewhere

	debug("predicting sensors:");

	for( i = 0; i < predct->numEvents ; i ++ ) {
		dist = sensor->dists[i] - mm ;

		debug( "%c%d, ", sensor_bank(sensor->idxs[i]),
				sensor_num(sensor->idxs[i]));

		predct->events[i].sensor = sensor->idxs[i];
		predct->events[i].start  = ticks + speed_time( lower, dist );
		predct->events[i].end    = ticks + speed_time( upper, dist );

		predct->expire = max( predct->expire, predct->events[i].end );
	}
	debug( "\r\n" );
	//if( req.expire > tr->trigger + SENSOR_TIMEOUT ) { }
//	debug( "train: req.expire=%dms in future\r\n", (req.expire - ticks) * MS_IN_TICK );
//	debug( "train: pr:%dmm/s \twi:%dmm/s \tlo:%dmm/s \tup:%dmm/s\r\n", 
//		speed( tr->velocity ), speed(window ), speed( lower ), speed( upper ) );
	
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
}

void train_drive( Train *tr, int gear ) {
	TSRequest req;
	TSReply	  reply;

	// Don't change your speed while calibrating
	if( tr->mode == CAL_SPEED && tr->gear != tr->defaultGear ) {
		gear = tr->defaultGear;
	} else if ( tr->mode == CAL_SPEED || (tr->mode == CAL_STOP && gear > tr->gear) ) {
		return;
	}
	
	// If we are already at this gear, just return.
	if( tr->gear == gear ) {
		return;
	}

	tr->gearChanged = true;

	if( gear >= 0 && gear < 15 ) {
		tr->gear = gear;
	}
	debug ("Train %d is driving at gear =%d\r\n", tr->id, gear);
		
	req.type = TR;
	req.train = tr->id;
	req.speed = gear;
	req.startTicks	= Time(tr->csTid);

	Send( tr->tsTid, &req, sizeof(TSRequest), &reply, sizeof(TSReply) );
}

void train_flipSwitches( Train *tr, RPReply *rpReply ) {
	TSRequest		req;
	TSReply			reply;
	SwitchSetting  *ss;

	// Do not change the switches if we are calibrating the speed
	if ( tr->mode == CAL_STOP ) return;

	if ( tr->mode == CAL_SPEED && tr->dest != INIT_DEST ) return;

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
	req.trainId = tr->id;
	Send( tr->uiTid, &req, sizeof(UIRequest), &tmp, sizeof(int) ); 
}

//-----------------------------------------------------------------------------
//----------------------------------- Heart -----------------------------------
//-----------------------------------------------------------------------------
void heart() {
	TID parent	= MyParentTid();
	TID csTid 	= WhoIs( CLOCK_NAME );
	TID deTid	= WhoIs( DETECTIVE_NAME ); 
	TrainReq 	beat;
	DeRequest	predct;

	beat.type  = TIME_UPDATE;
	FOREVER {
		Send( parent, &beat, sizeof(TrainReq), &predct, sizeof(DeRequest) );

		// Tell the detective the updated info
        Send( deTid, &predct, sizeof( DeRequest ), 0, 0 );
		
		// Heart beat for the train
		beat.ticks = Delay( HEARTBEAT_TICKS , csTid );
	}
}

//-----------------------------------------------------------------------------
//--------------------------------- Calibration -------------------------------
//-----------------------------------------------------------------------------
void calibration () {
	int 	 dests[] = { 37, 23, 15, 1, 22, 27 };
	int 	 i, j;
	TrainReq req;
	TID 	 parent = MyParentTid();

	// Find D9
	printf ("CALIBRATION: Finding D9\r\n");
	req.type = DEST_UPDATE;
	req.dest = INIT_DEST;		// D9
	req.mode = CAL_SPEED;	
	Send( parent, &req, sizeof(TrainReq), 0, 0 );
	
	// Travel in a loop
	for ( i = 0; i < CAL_LOOPS; i ++ ) {
		for ( j = 0; j < 6; j ++ ) {
			printf ("CALIBRATION: Train finding %d\r\n", dests[j]);
			req.dest = dests[j];

			Send( parent, &req, sizeof(TrainReq), 0, 0 );
		}
	}

	req.dest = 37;				// E11
	req.mode = CAL_STOP;		
	Send( parent, &req, sizeof(TrainReq), 0, 0 );

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
		Reply  ( trainTid, 0, 0 );

		// Wait a bit
		req.ticks = Delay( 500 / MS_IN_TICK, clockTid );

		// Wake up the train with a fake position update
		req.sensor = lastHitSensor;
		
		printf ("routeWatchDog: Waking up the train. There may be a path.\r\n");
		Send( trainTid, &req, sizeof(TrainReq), 0, 0 );
	}

	Exit(); // This task will never reach here
}
