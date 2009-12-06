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
#include "reservation.h"
#include "routeplanner.h"
#include "servers.h"
#include "shell.h"
#include "trackserver.h"
#include "train.h"
#include "ui.h"

#define	SPEED_HIST				10
#define	HEARTBEAT_MS			200
#define	HEARTBEAT_TICKS			(HEARTBEAT_MS / MS_IN_TICK)
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
#define REVERSE_DIST			400

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

	int  		destsBuf[80];	// Desired End Location
	RB			dests;			// A list of destinations
	TrainMode	mode;			// Train Mode (calibration or normal)
		
	int 	    gear;			// The current speed setting
	Speed		velocity;		// The actual current speed (in mm/s)
	int			stopDist;		// The measured stopping distance		
	
	TID			caTid;			// ID of the calibration task
	TID			csTid;			// ID of the Clock Server
	TID			resTid;			// ID of the Reservation Server
	TID 		rpTid;			// ID of the Route Planner
	TID			rwTid;			// ID of the route watch dog task
	TID			shTid;			// ID of the shell
	TID			uiTid;			// ID of the UI server
	TID 		tsTid;			// ID of the Track Server

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

inline Speed 
speed_sub( Speed sp1, Speed sp2 ) {
	sp2.mm = - sp2.mm;
	return speed_add( sp1, sp2 );
}

void 		train_init    		( Train *tr );
Speed 		train_avgSpeed		( Train *tr );
void 		train_adjustSpeed	( Train *tr, int distLeft, enum StopAction action );
int 		train_getStopDist	( Train *tr, int gear );
Speed 		train_getSpeed		( Train *tr, int gear );

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
void		train_flipSwitches	( Train *tr, RPReply *rpReply, int dist );
SwitchDir	train_dir			( Train *tr, int sw );

// Reservation Server Commands		
int 		train_makeReservation( Train *tr, int distPast, int totalDist );

// UI Server
void 		train_updateUI		( Train *tr, int mm );

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
	int			prevMode  = 0;
	int			safeDist  = 0;
	int			totalDist = 0;
	int			lastDistFromSensor = 0;

	predct.tid = MyTid();

	// Initialize this train
	train_init ( &tr );

	// The train should figure out where it is
	train_locate( &tr, timeout, &predct );
	
	FOREVER {
		// Receive a server request
		len = Receive ( &senderTid, &req, sizeof(req) );
		assert( len == sizeof(req) );

		switch (req.type) {

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

				// Make sure the route watcher is not blocked on the train.
				if ( routeWatcherAwake == true ) {
					routeWatcherAwake = false;
					Reply( tr.rwTid, 0, 0 );
				}

				// Update the internal dest variable
				prevMode = tr.mode;
				tr.mode	 = req.mode;
				rb_push( &tr.dests, &req.dest );
				
				// If you are not moving, start!
				if ( prevMode != IDLE  ) {
					break;
				}

			case POS_UPDATE: 		// got a position update
			//	printf("train: POS UP #%d is at sensor %c%d(%d)(node=%d). ",
			//			tr.id, sensor_bank( req.sensor ), sensor_num(req.sensor ),
			//			req.sensor, sIdxToIdx(req.sensor) );
			//	if ( rb_empty(&tr.dests) == 0 ) {
			//		printf("Going to %d\r\n", *(int*)rb_top(&tr.dests));
			//	}
			//	else printf("\r\n");
				
				// Do not reply twice for a DEST_UPDATE
				if ( req.type == POS_UPDATE ) {
					Reply( senderTid, 0, 0 );
					
					// Update the train's information
					train_updatePos( &tr, req.sensor, req.ticks );
				}
					
				// Reset the distance counter
				distFromSensor = 0;

				// Reset the timeout value
				timeout = LOCATE_TIMEOUT;
				
				// If we don't have a destination, don't do anything.
				if ( rb_empty( &tr.dests ) == 1 ) {
				//	printf ("We don't have any dests.\r\n");
					break;
				}

				// Ask the Route Planner for an efficient route!
				rpReply = train_planRoute ( &tr );

				// If there is no path available,
				if ( rpReply.err < NO_ERROR && tr.sensor != START_SENSOR ) {
					debug ("The train has no route. Stopping at %d.\r\n", tr.sensor);
					
					rpReply.stopDist        = 0;
					rpReply.stopAction      = JUST_STOP;
					rpReply.nextSensors.len = 0;
					
					// Stop moving
					train_adjustSpeed( &tr, rpReply.stopDist, rpReply.stopAction );

					printf("train: waking up routewatcher.\r\n");
					// Wake up the route watcher so that we can try again later
					// Non-Blocking
					routeWatcherAwake = true;
					Send( tr.rwTid, &tr.sensor, sizeof(int), 0, 0 );

					break;
				}
				
				// Reverse if the train is backwards
				if( rpReply.stopDist < 0 ) {
					debug ("Train is backwards. It's reversing.\r\n");
					// TODO : possibly have a reversing mode here and don't 
					// reverse if already in reversing mode
					train_reverse( &tr );			// TODO: Timeout incorrect for this mode.
					rpReply.stopDist = -rpReply.stopDist + REVERSE_DIST;

					// TODO: Is this correct?
					// Flip the last hit sensor
					//tr.sensor ^= 1;
				}
				if( rpReply.stopAction == STOP_AND_REVERSE ) {
					rpReply.stopDist += REVERSE_DIST;
				}

				// Save the total travelling distance
				totalDist = rpReply.stopDist;
				printf ("THE TOTAL DISTANCE TO THE DEST IS:%d\r\n", totalDist);

				// If we have reached our destination, 
				if ( totalDist == 0 ) {
					if ( tr.mode == CAL_SPEED || tr.mode == CAL_STOP ) {
						// We've reached our destination,
						// Go to the next OR wait to get a new destination
						printf( "Train %d reached %d. ", tr.id, 
								*(int*)rb_top(&tr.dests) );

						// Take this destination off the queue.
						rb_pop( &tr.dests );
						
						if( rb_empty(&tr.dests) == 0 ) {
							printf ("\r\nGoing to %d.\r\n", 
									*(int*)rb_top(&tr.dests) );
						}
							
						if ( tr.mode == CAL_SPEED  ) {
							debug ("replying to the calibrator\r\n");
							// Reply to the Calibration task
							Reply( tr.caTid, 0, 0 );
						} else if ( tr.mode == CAL_STOP ) {
							debug ("cal stopped. replying to the shell.\r\n");
							// Reply to the shell
							Reply( tr.shTid, 0, 0 );
						}
					//break;
					}
				}

				// Predict the sensors we are about to hit.
				train_predictSensors( &tr, &rpReply.nextSensors, distFromSensor, &predct );

			case TIME_UPDATE:		// heartbeat
				// Update current location (NOTE: This is an estimate!)
				distFromSensor += speed_dist( tr.velocity, HEARTBEAT_MS );
				
				if ( tr.mode == IDLE ) {
					assert( speed(tr.velocity) == 0 );
					assert( speed_dist(tr.velocity, HEARTBEAT_MS) == 0 );
				}

				// Send this information to the UI
				train_updateUI( &tr, distFromSensor );
				
				// Only update your reservation every 20 mm OR 
				// when you are stopping OR
				// when you are starting
			//	if ( (distFromSensor > lastDistFromSensor + 20) || (totalDist == 0) ||
			//		 (prevMode == IDLE && tr.mode == DRIVE) ) {
			//		lastDistFromSensor = distFromSensor;

					// Make a reservation
					if ( tr.mode != CAL_SPEED && tr.mode != CAL_STOP ) {
						safeDist = train_makeReservation(&tr, distFromSensor, totalDist); 
					} else {
						safeDist = train_makeReservation(&tr, 0, totalDist); 
					}
			//		debug("safe=%d total=%d\r\n", safeDist, totalDist);
			//	}

				// Adjust speed
				// Yes, this uses the PREVIOUS safe distance
				prevMode = tr.mode;
				train_adjustSpeed( &tr, safeDist, rpReply.stopAction );
				
				if ( prevMode == DRIVE && tr.mode == IDLE ) {
					// We've reached our destination,
					// Go to the next OR wait to get a new destination
					printf ("Train %d reached %d. ", tr.id, *(int*)rb_top(&tr.dests));

					// Take this destination off the queue.
					rb_pop( &tr.dests );
					
					if( rb_empty(&tr.dests) == 0 ) {
						printf ("Going to %d.\r\n", *(int*)rb_top(&tr.dests) );
					}
				} else if( tr.mode != IDLE ) {
					// Flip the switches along the path.
					train_flipSwitches( &tr, &rpReply, safeDist );
				}
				
				// Only update predictions if train is not lost 
				if( predct.type == WATCH_FOR ) {
					// Resend detective request
					train_predictSensors( &tr, &rpReply.nextSensors, 
							distFromSensor, &predct );
				}
				
				if ( req.type == TIME_UPDATE ) {
					Reply( senderTid, &predct, sizeof(DeRequest) );
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
	
	Create( TRAIN_PRTY - 1, &heart );

	// The train talks to several servers
	tr->csTid  = WhoIs (CLOCK_NAME);
	tr->resTid = WhoIs (RESERVATION_NAME);
	tr->rpTid  = WhoIs (ROUTEPLANNER_NAME);
	tr->tsTid  = WhoIs (TRACK_SERVER_NAME);
	tr->uiTid  = WhoIs (UI_NAME);
	
	tr->caTid  = Create( TRAIN_PRTY - 1, &calibration );
	tr->rwTid  = Create( TRAIN_PRTY - 1, &routeWatchDog );
	
	assert( tr->caTid  >= NO_ERROR );
	assert( tr->csTid  >= NO_ERROR );
	assert( tr->rpTid  >= NO_ERROR );
	assert( tr->resTid >= NO_ERROR );
	assert( tr->tsTid  >= NO_ERROR );
	assert( tr->uiTid  >= NO_ERROR );

	// Initialize internal variables
	tr->mode   = CAL_SPEED;
	tr->sensor = START_SENSOR;
	tr->gear   = 0;

	// Initialize the destination data
	rb_init( &(tr->dests), tr->destsBuf );
	memoryset( tr->destsBuf, START_SENSOR, sizeof(tr->destsBuf) );

	// Initialize the calibration data
	rb_init( &(tr->hist), tr->histBuf );
	memoryset( tr->histBuf, 0, sizeof(tr->histBuf) );
	Speed sp = {350, 1000}; // inital speed
	rb_push( &(tr->hist), &sp );

	tr->velocity     = train_avgSpeed (tr);
	tr->trigger      = Time( tr->csTid );
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
	assert( distLeft >= 0 );
	
	int bestGear = tr->defaultGear;
	int stopDist = 0;
	int i;
	
	// If we are not in calibration mode,
	if( tr->mode == DRIVE ) {
		// Given the stop distance for this train,
		// estimate the best gear for the train to travel at.
		// In theory, as distLeft decreases, so does the speed.
		for ( i = tr->defaultGear; i >= 1; i -- ) {
			stopDist = train_getStopDist( tr, i );

			// Multiply by x to account for acc/deceleration
			if ( stopDist <= distLeft ) {
				//printf( "predicted stopping distance for gear %d is %dmm\r\n", i, stopDist );
//				printf ("new Gear=%d old gear=%d\r\n", i, tr->gear);
				bestGear = i;
				break;
			}
		}

		if ( action == STOP_AND_REVERSE && bestGear < 3 ) {
			bestGear = 3;
		}
		if ( action == JUST_STOP && distLeft <= 0 ) {//If we need to stop,
			bestGear = 0;
			tr->mode = IDLE;
		}
		
	//	printf ("picking %d as best gear for %d left.\r\n", bestGear, distLeft);
	} 

	// If you are stopping calibration, or in idle mode
	if ( (tr->mode == CAL_STOP && distLeft == 0 && rb_empty(&tr->dests) )
		|| (tr->mode == IDLE) ) {
		tr->mode = IDLE;		// Assert this state!
		bestGear = 0;
	}

	// Change your speed to this new gear.
	train_drive( tr, bestGear );

	// Adjust calibrated speed based on gear
	Speed set = train_getSpeed( tr, tr->gear );

	Speed accel = { HEARTBEAT_MS, 10000 }; // 100 mm/s^2
	// The current speed does not change rapidly
	int diff = speed( set ) - speed( tr->velocity );
	if( diff > speed( accel ) ) {
		tr->velocity = speed_add( tr->velocity, accel ); // positive accel
	} else if( diff >= -speed( accel ) ) {
		tr->velocity = set; 							// minimal accel
	} else {
		tr->velocity = speed_sub( tr->velocity, accel ); // negative accel
	}
	tr->velocity = set;

	if ( tr->mode == IDLE ) {
		assert( speed(tr->velocity) == 0 );
		assert( speed_dist(tr->velocity, HEARTBEAT_MS) == 0 );
	}
//	debug( "train: speed adjusted to %dmm/s\r\n", speed( tr->velocity ) );
}

// Returns the stop distance in mm given the gear and train id
int train_getStopDist( Train *tr, int gear ) {
	int stopDist;
	int calGear = 14;
	int calDist;
	// Use the stopping distance multiplier to better predict
	// TODO
	// calGear = tr->defaultGear
	// calDist = tr->stopDist
	
	switch( tr->id ) {
		case 52:
		case 12:
			// These trains are Special, ladee dadee daah
			// CRAZY squared profiles
			calDist = 1320;
			stopDist = calDist * (gear * gear) / (calGear * calGear);
			break;
		default: // train 15,22,24 - NORMAL trains with linear profiles
			calDist = 970;
			stopDist = calDist * gear / calGear;
			break;
	}

	return stopDist;
}

// returns a number to it's 1.75 power
inline int pow_1_75 ( int x ) {
	int x2 = x * x;
	int x4 = x2 * x2;
	int x7 = x4 * x2 * x;

	return isqrt( isqrt( x7 ) );
}

// Returns the train speed in mm/s
Speed train_getSpeed ( Train *tr, int gear ) {
	Speed base = train_avgSpeed( tr );
	int calGear = tr->defaultGear;

	switch( tr->id ) {
		case 52:
		case 12:
			// These trains are Special, ladee dadee daah
			// CRAZY ^1.75 profiles
			//return speed_adjust( base, pow_1_75( calGear ), pow_1_75( gear ) );
		default: // train 15,22,24 - NORMAL trains with linear profiles
			return speed_adjust( base, calGear, gear );
	}
}


void train_updatePos( Train *tr, int sensor, int ticks ) {
	int 		diff;
	Speed		sp;

	// Calculate the average speed over the last segment
	sp.ms = ( ticks - tr->trigger ) * MS_IN_TICK;
	sp.mm = train_distance( tr, sensor );
	
	// Update the mean speed
	if( tr->mode == CAL_SPEED ) {
		printf ( "adding speed %d/%d to history data\r\n", sp.mm, sp.ms );
		
		rb_force( &tr->hist, &sp );
	}

	// See how far we were off
	diff = speed_dist( tr->velocity, sp.ms ) - sp.mm;

	if( abs(diff) > 100 ) 		{ printf("\033[31m"); }
	else if( abs(diff) > 50 ) 	{ printf("\033[33m"); }
	else if( abs(diff) > 20 ) 	{ printf("\033[36m"); }
	else 						{ printf("\033[32m"); }
	printf("%d/%d = \t%dmm/s \tmu:%dmm/s \tsd:%dmm/s: \tdiff:%dmm\033[37m\r\n",
			sp.mm, sp.ms, speed( sp ), speed( tr->velocity ) , tr->sd, diff);
	
	tr->sensor  = sensor; 
	tr->trigger = ticks;
}

//-----------------------------------------------------------------------------
//-------------------------- Route Planner Commands ---------------------------
//-----------------------------------------------------------------------------
int train_distance( Train *tr, int sensor ) {
	RPRequest	req;
	int	dist;
	
	if( tr->sensor == START_SENSOR ) {
		return 0;
	}

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

	if( tr->sensor == START_SENSOR ) {
		reply.err = INVALID_SENSOR_IDX;
		return reply;
	}

	// Initialize the Route Planner request
	req.type		= PLANROUTE;
	req.trainId		= tr->id;
	req.lastSensor	= tr->sensor;
	req.destIdx		= *(int *)rb_top( &tr->dests );
	req.avgSpeed	= min( speed( tr->velocity ), 0 );

//	printf ("planning a route from %d to %d\r\n", req.lastSensor, req.destIdx );

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
	
	// Drive at locate speed
	train_drive( tr, LOCATE_GEAR );
}

void train_predictSensors( Train *tr, SensorsPred *sensor, int mm, DeRequest * predct ) {
	if ( tr->mode == IDLE ) return;		// Do not predict if you are not moving.
	
	int 		i, dist, ticks = Time( tr->csTid );
	int			variation = max( tr->sd, speed( tr->velocity )/ SD_THRESHOLD );

	Speed		window = { variation * 3 , 1000 }, upper, lower;
	lower		= train_getSpeed( tr, tr->gear );
	upper		= lower;
	upper		= speed_add( upper, window );
	lower 		= speed_sub( lower, window );

	assert( sensor->len <= array_size( predct->events ) );
	
	predct->type 		= WATCH_FOR;
	predct->numEvents	= sensor->len;
	predct->expire 		= 0;

//	debug("predicting sensors:");

	for( i = 0; i < predct->numEvents ; i ++ ) {
		dist = sensor->dists[i] - mm ;

//		debug( "%c%d, ", sensor_bank(sensor->idxs[i]),
//				sensor_num(sensor->idxs[i]));

		predct->events[i].sensor = sensor->idxs[i];
		predct->events[i].start  = ticks + speed_time( lower, dist );
		predct->events[i].end    = ticks + speed_time( upper, dist );

		predct->expire = max( predct->expire, predct->events[i].end );
	}
//	debug( "\r\n" );
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

	if( gear >= 0 && gear < 15 ) {
		tr->gear = gear;
	}
	debug ("Train %d is driving at gear =%d\r\n", tr->id, gear);
		
	req.type = TR;
	req.train = tr->id;
	req.speed = tr->gear;
	req.startTicks	= Time(tr->csTid);

	Send( tr->tsTid, &req, sizeof(TSRequest), &reply, sizeof(TSReply) );
}

void train_flipSwitches( Train *tr, RPReply *rpReply, int reserveDist ) {
	TSRequest		req;
	TSReply			reply;
	SwitchSetting  *ss;

	// Do not change the switches if we are calibrating the speed
	if ( tr->mode == CAL_STOP ) return;

	foreach( ss, rpReply->switches ) {
	  	if( ss->id <= 0 ) break;

		// Only flip switches in your reserved section
	 	if ( reserveDist <= 0 ) break;

	  	// only flip switches in the inproper position
	  	if ( train_dir( tr, ss->id ) != ss->dir ) {
		//	debug("train: flip sw%d to %c\r\n", ss->id, switch_dir(ss->dir));
			req.type 		= SW;
			req.sw   		= ss->id;
			req.dir  		= ss->dir;
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
//--------------------------------- Reservation Server ------------------------
//-----------------------------------------------------------------------------
int train_makeReservation( Train *tr, int distPast, int totalDist ) {
	ResRequest  req;
	ResReply	reply;

	req.type      = TRAIN_MAKE;
	req.trainId   = tr->id;
	req.sensor    = tr->sensor;
	req.distPast  = distPast;
	req.totalDist = totalDist;
	
	req.stopDist  = train_getStopDist( tr, tr->gear );
	if( req.stopDist == 0 ) {
		req.stopDist = train_getStopDist(tr, tr->defaultGear)*2 + 10;
	}
	
	Send( tr->resTid, &req, sizeof(ResRequest), &reply, sizeof(ResReply) );
	return reply.stopDist;
	int diff = totalDist - distPast;
	if( diff < 0 ) return 0;
	return diff; //reply.stopDist;
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
	int 	 dests[] = 
	//{27, 35};
	{ 37, 23, 15, 1, 22, 27 };
	int 	 i, j;
	TrainReq req;
	TID 	 parent = MyParentTid();

	printf ("CALIBRATION: Finding D9\r\n");
	req.type = DEST_UPDATE;
	req.dest = INIT_DEST;           // D9
	req.mode = CAL_SPEED;   
	Send( parent, &req, sizeof(TrainReq), 0, 0 );

	// Travel in a loop
	for ( i = 0; i < CAL_LOOPS; i ++ ) {
	
		for ( j = 0; j < array_size(dests); j ++ ) {
			
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
		req.ticks = Delay( 750 / MS_IN_TICK, clockTid );

		// Wake up the train with a fake position update
		req.sensor = lastHitSensor;
		
		printf ("routeWatchDog: Waking up the train sensor=%d\r\n", lastHitSensor);
		Send( trainTid, &req, sizeof(TrainReq), 0, 0 );
	}

	Exit(); // This task will never reach here
}
