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
#define PREDICTION_WINDOW 		(1000 /MS_IN_TICK)
#define	BLANK_RECORD_DIVISOR	2
#define	HEARTBEAT				(200 /MS_IN_TICK)
#define	TIME_UPDATE				-1
#define WATCHMAN_PRIORITY		3
#define CALIBRATION_DEST		37 // E11 
#define INIT_DEST				28 // D9
#define LOCATE_TIMEOUT			(7000 /MS_IN_TICK)
#define	INIT_GRACE				(10000 /MS_IN_TICK)
#define INIT_GEAR				5
#define SD_THRESHOLD			10	// parts of mean	
#define CAL_LOOPS				2
#define	C						4000
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

inline int speed( Speed sp ) { // in mm/s
	if( sp.ms == 0 ) return 0;
	return (sp.mm * 1000) / sp.ms;
}

inline int	speed_time( Speed sp, int mm ) { // in ms
	return (sp.ms * mm) / sp.mm;
}

inline int speed_dist( Speed sp, int ms ) { // in mm
	return (sp.mm * ms) / sp.ms;
}


void 		train_init    		( Train *tr );
Speed 		train_speed			( Train *tr );
void 		train_predictSpeed	( Train *tr, RPReply *rpReply );

// Shell Commands
void 		train_getPurpose	( Train *tr );
void 		train_getStopDist	( Train *tr );

// Route Planner Commands
RPReply		train_planRoute 	( Train *tr );
int 		train_distance		( Train *tr, int sensor ); // in mm

// Detective Commands
void 		train_locate		( Train *tr );
void	 	train_check			( Train *tr, RPReply *rep );
void 		train_update		( Train *tr, DeReply *rpl );

// Track Server Commands
void 		train_reverse		( Train *tr );
void 		train_drive			( Train *tr, int gear );
void		train_flipSwitches	( Train *tr, RPReply *rpReply );
SwitchDir	train_dir			( Train *tr, int sw );

// Watchman
void 		train_watch 		( Train *tr, RPReply *rep );
void		heart				();
void		courier				();
// ----------------------------------------------------------------------------

void train_run () {
	Train 	 	tr;
	RPReply   	rpReply;		// Reply from the Route Planner

	// Initialize this train
	train_init ( &tr );

	// Locate the train
	train_locate( &tr );

	tr.dest = INIT_DEST;
	debug( "train: found itself at %c%d\r\n", 
			sensor_bank(tr.sensor), sensor_num(tr.sensor) );

	FOREVER {

		// Ask for a destination
		if( tr.mode == IDLE ) {
			debug("train: awaiting purpose\r\n");
			train_getPurpose( &tr );
			// TODO hack?
			tr.velocity.mm = 50;
			tr.velocity.ms = 1000;
		}

		// Until you've reached your destination:
		FOREVER {
		
			// Ask the Route Planner for an efficient route!
			rpReply = train_planRoute ( &tr );
			//debug( "train: has a route stopDist=%d\r\n", rpReply.stopDist );

			if( rpReply.err < NO_ERROR ) {
				//eprintf( "Route Planner failed (%d).\r\n", rpReply.err );
				rpReply.stopDist = 0; 
				rpReply.switches[0].id = -1;
				rpReply.nextSensors.len = 0;
			}

			// Set the speed to an appropriate setting
			train_predictSpeed( &tr, &rpReply );
			
			if ( tr.mode == IDLE ) {
				debug ("Train is now idle\r\n");
				break;
			}
			
			// Flip the switches along the path.
			train_flipSwitches( &tr, &rpReply );

			// Tell the detective about your predicted sensors
			train_watch( &tr, &rpReply );
			
			if ( tr.mode == IDLE ) {
				debug ("Train has properly reached its destination.\r\n");
				break;
			}
		}

	}

	// Train task never exits
	assert( 1 == 0 );
}

void train_init ( Train *tr ) {

	TID tid;
	TrainInit init;
	
	// Get the train number from shell
	Receive ( &tid, (char*)&init, sizeof(TrainInit) );
	
	// Copy the data to the train
	tr->id   		= init.id;
	tr->defaultGear = init.gear;

	// Reply to the shell
	Reply   ( tid, (char*)&tr->id, sizeof(int) );

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

	DeReply tmp;
	Receive	( &tid, (char *) &tmp, sizeof( DeReply ) );
	
	// Initialize internal variables
	tr->mode = INIT_LOC;
	tr->sensor = 0;
	tr->gear = 0;

	// Initialize the calibration data
	rb_init( &(tr->hist), tr->histBuf );
	memoryset( (char *) tr->histBuf, 0, sizeof(tr->histBuf) );
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
		if( rec->mm > 0 ) {
			var += tmp * tmp;
		} else {
			var += (tmp * tmp) / BLANK_RECORD_DIVISOR;
		}
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


void train_predictSpeed( Train *tr, RPReply *rep ) {

	// TODO: PLACE speed adjusting here
	int gear = tr->defaultGear;

	// If you should reverse / are backwards, turn around.
	if( rep->stopDist < 0 ) {
		train_reverse( tr );			// TODO this
		rep->stopDist = -rep->stopDist;

	} else if( rep->stopDist == 0 ) { 


		debug( "train: Reached its destination.\r\n" );

		// Depending on the mode do different things
		switch( tr->mode ) {
			case INIT_LOC:
				tr->mode = INIT_DIR;
				tr->dest = CALIBRATION_DEST;
				break;
			case INIT_DIR:
				tr->mode = CAL_SPEED;
				break;
			case CAL_SPEED:
				tr->cal_loops ++;
				break;
			case CAL_STOP:
				train_drive( tr, 0 );
				printf("train: stopping at E11\r\n");
				train_getStopDist( tr );
			default:
				tr->mode = IDLE;
				gear = 0;
				break;
		}
	}
	train_drive( tr, gear);
}


void train_update( Train *tr, DeReply *rpl ) {
	int 		last, avg, diff, dist, time;
	Speed		sp, pr;

	// Calculate the distance traveled
	dist = train_distance( tr, rpl->sensor );
	tr->sensor = rpl->sensor; 

	
	sp.ms = (rpl->ticks - tr->trigger) * MS_IN_TICK;
	sp.mm = dist;
	last = speed( sp );
	
	// Calculate the mean speed
	pr = train_speed( tr );
	avg = speed( pr );
	
	// Update the mean speed
	if( ! tr->gearChanged ) {
		debug( "adding speed %d to history data\r\n");
		rb_force( &tr->hist, &sp );
	}
	tr->gearChanged = false;
	diff = ((avg * sp.ms) - (sp.mm * 1000)) / 1000;

	if( abs(diff) > 100 )
		printf("\033[31m");
	else if( abs(diff) > 50 )
		printf("\033[33m");
	else if( abs(diff) > 20 )
		printf("\033[36m");
	else 
		printf("\033[32m");
	printf("%d/%d = \t%dmm/s \tmu:%dmm/s \tsd:%dmm/s: \tdiff:%dmm\033[37m\r\n",
			sp.mm, sp.ms, last, avg, tr->sd, diff);

		
	time = (Time( tr->csTid ) - tr->lastStopTime) * MS_IN_TICK; 
	// calculate acceleration
	printf("train: acceleration is %dmm/s^2\r\n", (last * 1000)/ time );
	
	tr->trigger = rpl->ticks;
	tr->velocity = sp;
}

//-----------------------------------------------------------------------------
//-------------------------- Shell Commands -----------------------------------
//-----------------------------------------------------------------------------

void train_getStopDist( Train *tr ) {
	// TODO replace 60 with p[roper number
	// TODO this doesn't work
	// harcode 650 mm
	tr->stopDist = 900;
	int dist;
	int senderTid;

	Receive( &senderTid, (char*)&dist, sizeof(int) );
	Reply  ( senderTid,  (char*)&dist, sizeof(int) );
//		printf( "Invalid stopping distance. Try again.\r\n" );

	tr->stopDist = dist;
	printf( "\r\n%dmm stored as stopping distance.\r\n", tr->stopDist );
}

//-----------------------------------------------------------------------------
//-------------------------- Shell Commands -----------------------------------
//-----------------------------------------------------------------------------

void train_getPurpose( Train *tr ) {
	int shellTid;
	TrainCmd cmd;
	
	// Get the train number from shell
	Receive ( &shellTid, (char*)&cmd, sizeof(TrainCmd) );
	
	// Copy the data to the train
	tr->dest   		= cmd.dest;
	tr->mode 		= cmd.mode;
	tr->trigger		= Time( tr->csTid );

	debug ("Train %d is at sensor %d going to destidx %d\r\n",
			tr->id, tr->sensor, tr->dest);

	// Reply to the shell
	Reply   ( shellTid, (char*)&tr->id, sizeof(int) );
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

	Send( tr->rpTid, (char*) &req,  sizeof(RPRequest),
			 		 (char*) &dist, sizeof(int) );
	

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

	Send( tr->rpTid, (char*) &req,   sizeof(RPRequest),
			 		 (char*) &reply, sizeof(RPReply) );
	
	return reply;
}

//-----------------------------------------------------------------------------
//-------------------------- Detective Commands -------------------------------
//-----------------------------------------------------------------------------

void train_locate( Train *tr ) {
	int			timeout = LOCATE_TIMEOUT;
	DeReply 	rpl;
	DeRequest	req;
	req.type = GET_STRAY;

	FOREVER {
		train_drive( tr, INIT_GEAR );

		req.expire = timeout + Time( tr->csTid );
		printf( "Train is lost, trying to find itself until %d.\r\n", req.expire );
		// Try again
		Send( tr->deTid, (char *) &req, sizeof(DeRequest),
						 (char *) &rpl, sizeof(TSReply) );
	  if( rpl.sensor >= NO_ERROR ) break;
		train_reverse( tr );
		// try for a longer time next time
		timeout *= 2;
	}

	train_update( tr, &rpl );
}

void train_retract( Train *tr ) {
	DeReply 	rpl;
	DeRequest	req;
	req.type	= RETRACT;
	req.tid  	= tr->coTid;

	// Retract the courier
	Send( tr->deTid, (char *) &req, sizeof(DeRequest),
					 (char *) &rpl, sizeof(TSReply) );
}

void train_check( Train *tr, RPReply *rep ) {
	DeRequest	req;
	int 		i;
	int			window = PREDICTION_WINDOW;

	req.type      = WATCH_FOR;
	req.numEvents = rep->nextSensors.len;
	req.expire 	  = 0;

//	if( rep.stopAction == STOP_AND_REVERSE ) 

	assert( req.numEvents < array_size( req.events ) );
	for( i = 0; i < req.numEvents ; i ++ ) {
		req.events[i].sensor = rep->nextSensors.idxs[i];
		req.events[i].start  = tr->trigger
			+ speed_time ( tr->velocity, rep->nextSensors.dists[i] ) - window;
		req.events[i].end    = req.events[i].start + window * 2;
		req.expire = max( req.expire, req.events[i].end );
	}
	// This awas a stupid idea
	req.expire = tr->trigger + 10000 / MS_IN_TICK;
	
	// Tell the detective about the Route Planner's prediction.
	Send( tr->coTid, (char *) &req,	sizeof(DeRequest), 0 , 0 );

}

//-----------------------------------------------------------------------------
//-------------------------- Track Server Commands ----------------------------
//-----------------------------------------------------------------------------
void train_reverse( Train *tr ) {
	TSRequest req;
	TSReply	  reply;
	
	if( tr->mode == CAL_SPEED || tr->mode == CAL_STOP ) return;

	tr->gearChanged = true;

	req.type = RV;
	req.train = tr->id;
	req.startTicks	= Time(tr->csTid);

	Send( tr->tsTid, (char *)&req,   sizeof(TSRequest),
					 (char *)&reply, sizeof(TSReply) );
	tr->lastStopTime == Time( tr->csTid );
}

void train_drive( Train *tr, int gear ) {
	debug ("Train %d is driving at gear =%d\r\n", tr->id, gear);
	TSRequest req;
	TSReply	  reply;

	//if( tr->mode == CAL_SPEED ) return;
	
	if( tr->gear != gear ) {
		tr->gearChanged = true;
	}
	if( gear >= 0 && gear < 15 ) {
		tr->gear = gear;
	}
		
	req.type = TR;
	req.train = tr->id;
	req.speed = gear;
	req.startTicks	= Time(tr->csTid);

	Send( tr->tsTid, (char *)&req,   sizeof(TSRequest),
					 (char *)&reply, sizeof(TSReply) );


	if( gear == 0 ) tr->lastStopTime == Time( tr->csTid );
}

void train_flipSwitches( Train *tr, RPReply *rpReply ) {
	TSRequest		req;
	TSReply			reply;
	SwitchSetting  *ss;
	int				err;

	if( tr->mode != NORMAL && tr->mode != INIT_LOC ) return;

	foreach( ss, rpReply->switches ) {
	  if( ss->id <= 0 ) break;
	//	debug("train: flip sw%d to %c\r\n", ss->id, switch_dir(ss->dir));
		req.type = SW;
		req.sw   = ss->id;
		req.dir  = ss->dir;
		req.startTicks	= Time(tr->csTid);

		err = Send( tr->tsTid, (char *)&req,   sizeof(TSRequest),
								(char *)&reply, sizeof(TSReply) );

	}
}

SwitchDir train_dir( Train *tr, int sw ) {
	TSRequest 	req = { ST, {sw}, {0} };
	TSReply		reply;
	
	req.startTicks	= Time(tr->csTid);

	Send( tr->tsTid, (char *)&req,	  sizeof(TSRequest), 
					  (char *)&reply, sizeof(TSReply) );
	return reply.dir;
}


//-----------------------------------------------------------------------------
//--------------------------------- Watchman ----------------------------------
//-----------------------------------------------------------------------------
//-- Question: Who watches the watchman? --------------------------------------

void train_watch( Train *tr, RPReply *rep ) {

	int ticks = 0;
	int dist, tmp, ticksToReverse; 
	bool stopped = false, reversed = false;
	UIRequest 	uiReq;
	DeReply		req;
	uiReq.type = TRAIN;
	TID			sender;
	int timeToStop = isqrt( speed( tr->velocity ) * C ); // in ms

	debug( "time to stop %dms\r\n", timeToStop );
	// Start off the heart task
	Reply	( tr->htTid, 0, 0 );
	
	// tell the watchman task to look for these sensors
	train_check( tr, rep );
	FOREVER {
		// Receive a either a reply from detective or send from heart
		Receive	( &sender, (char *) &req, sizeof( DeReply ) );
		Reply	( sender, 0, 0 );

		if( req.sensor != TIME_UPDATE ) {
			printf( "got a detective reply back, %d\r\n", req.sensor );
			if( req.sensor >= NO_ERROR || tr->mode == IDLE ) {
				train_update( tr, &req );
			} else {
				// Train is lost
				train_locate( tr );
			}
			// Stop the heart
			Receive	( &sender, (char *) &req, sizeof( DeReply ) );
			return;
		} else {
			ticks = req.ticks;
		}

		// Update current location
		dist = speed_dist( tr->velocity, (ticks - tr->trigger) * MS_IN_TICK );
		
		// Send this information to the UI
		uiReq.idx  = tr->sensor;
		uiReq.dist = dist;
		Send( tr->uiTid, 	(char*) &uiReq, sizeof(UIRequest), 
							(char*)	&tmp, 	sizeof(int) ); // filler ...



		// See if we have to stop
		if( tr->mode == NORMAL ) {
			// Stop the train if needed
			if( ( rep->stopDist - dist ) < tr->stopDist 
					//(speed_time( tr->velocity, rep->stopDist - dist ) < timeToStop) 
					&& !stopped ) {
				ticksToReverse = (timeToStop/ MS_IN_TICK) + ticks;
				printf( "\033[42m %dms \033[49m\r\n", ticksToReverse * MS_IN_TICK);
			//printf( "watchman ticks%d, trigger%d\r\n", ticks, tr->trigger);
				printf( "\033[41m stopping %dmm \033[49m\r\n", 
						rep->stopDist - dist );

				// do not stop again
				stopped = true;

				// we've actualyl reached our destination
				if( rep->stopAction == JUST_STOP ) {
					// Stop the train
					train_drive( tr, 0 ); 
					tr->mode = IDLE;
					//replace with trai_honk
					train_drive( tr, 68 );
					train_drive( tr, 60 );
				}
			}
/*
				
			// The train should be stopped, reverse it
			if( stopped && !reversed && (ticksToReverse < ticks)
					&& (rep->stopAction == STOP_AND_REVERSE) ) {
				printf ( "\033[41m reversing \033[49m\r\n" );
				train_drive( tr, 15 );
				train_drive( tr, tr->defaultGear );
				// TODO hack?
				tr->velocity.mm = 50;
				tr->velocity.ms = 1000;
				reversed = true;
			}*/
		}

		/*if ( ticks > tr->trigger + LOCATE_TIMEOUT ) {
			// Retract the sensor poll
			train_retract( tr );
		}*/
	} 
}



void courier() {
	debug( "courier starting\r\n");
	TID 		deTid = WhoIs (DETECTIVE_NAME);
	TID			trTid;
	DeRequest 	req;
	DeReply		rpl;

	assert( deTid >= NO_ERROR );

	FOREVER {
		debug( "courier receiving\r\n");
		// Reveice a detective request
		Receive	( &trTid, (char *) &req, sizeof( DeRequest ) );
		Reply	(  trTid, 0, 0 );

		// Ask the detective
		Send	(  deTid, (char *) &req, sizeof( DeRequest ), 
						  (char *) &rpl, sizeof( DeReply ) );

		// Give the result back to the train
		Send	(  trTid, (char *) &rpl, sizeof( DeReply ), 0, 0 );
	}
}


void heart() {
	TID parent	= MyParentTid();
	TID csTid 	= WhoIs( CLOCK_NAME );
	DeReply		rpl;
	rpl.sensor  = TIME_UPDATE;

	FOREVER {
		Send( parent, (char *) &rpl, sizeof(DeReply), 0, 0 );
		// Heart beat for the watchman
		rpl.ticks = Delay( HEARTBEAT, csTid );
	}
}

