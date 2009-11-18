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
#include "requests.h"
#include "routeplanner.h"
#include "servers.h"
#include "trackserver.h"
#include "train.h"
#include "ui.h"

#define	SPEED_HIST				10
#define PREDICTION_WINDOW 		(1000 /MS_IN_TICK)
#define	BLANK_RECORD_DIVISOR	2
#define	HEARTBEAT				(200 /MS_IN_TICK)
#define WATCHMAN_PRIORITY		3
#define CALIBRATION_DEST		37 // E11 
#define INIT_DEST				28 // D9
#define LOCATE_TIMEOUT			(5000 /MS_IN_TICK)
#define	INIT_GRACE				(10000 /MS_IN_TICK)
#define INIT_GEAR				6
#define SD_THRESHOLD			10	// parts of mean	
#define CAL_LOOPS				2
#define	C						10 / 4
// Private Stuff
// ----------------------------------------------------------------------------

typedef struct {
	int 		mm;
	int 		ms;
} Speed;

typedef struct {
	int			ticks;			// used by the heartbeat
	int			id;				// Identifyin train number
	TrainMode	mode;			// Train Mode (calibration or normal)
	enum StopAction stopAction; // Wha the train is supposed to do at the stop
	int			sensor;			// last landmark
	int			crashDist;		// in mm
	int			minDist;		// in mm
	int			gear;			
	Speed		velocity;		// The actual current speed (in mm/s)
	Speed		topVel;			// The top speed for the current speed setting

} Disaster;

typedef struct {
	int  		id;				// Identifying number
	int			defaultGear;	// Default speed setting
	int  		currSensor;		// Current Location
	int	 		prevSensor;		// Previous Location
	int  		dest;			// Desired End Location
	TrainMode	mode;			// Train Mode (calibration or normal)
		
	int 	    gear;			// The current speed setting
	bool		speedChanged;	// Has the speed changed
	Speed		velocity;		// The actual current speed (in mm/s)
	
	TID 		rpTid;			// Id of the Route Planner
	TID 		tsTid;			// Id of the Track Server
	TID			deTid;			// Id of the Detective
	TID			csTid;			// Id of the Clock Server
	TID			waTid;			// Id of the Watchman

	int			cal_loops;
	int			ticks;
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

// Route Planner Commands
RPReply		train_planRoute 	( Train *tr );
int 		train_distance		( Train *tr ); // in mm

// Detective Commands
void 		train_locate		( Train *tr );
void	 	train_wait			( Train *tr, RPReply *rep );
void 		train_update		( Train *tr, DeReply *rpl );

// Track Server Commands
void 		train_reverse		( Train *tr );
void 		train_drive			( TID tsTid, int trainId, int gear );
void		train_flipSwitches	( Train *tr, RPReply *rpReply );
SwitchDir	train_dir			( Train *tr, int sw );

// Watchman
void 		train_watch 		( Train *tr, RPReply *rep );
void		watchman			();
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
			sensor_bank(tr.currSensor), sensor_num(tr.currSensor) );

	FOREVER {

		// Ask for a destination
		if( tr.mode == IDLE ) {
			debug("train: awaiting purpose\r\n");
			train_getPurpose( &tr );
		}

		// we are stopped, so discount the speed
		tr.speedChanged = true;
		
		// Until you've reached your destination:
		FOREVER {
		
			// Ask the Route Planner for an efficient route!
			rpReply = train_planRoute ( &tr );
			debug( "train: has a route stopDist=%d\r\n", rpReply.stopDist );

			if( rpReply.err < NO_ERROR ) {
				//eprintf( "Route Planner failed (%d).\r\n", rpReply.err );
				rpReply.stopDist = 0; 
				rpReply.switches[0].id = -1;
				rpReply.nextSensors.len = 0;
			}

			// Set the speed to an appropriate setting
			train_predictSpeed( &tr, &rpReply );
			
			// Set up the watchman to stop the train if neccesary
			train_watch( &tr, &rpReply );

			if ( tr.mode == IDLE ) {
				debug ("Train is now idle\r\n");
				break;
			}
			
			// Flip the switches along the path.
			train_flipSwitches( &tr, &rpReply );

			// Tell the detective about your predicted sensors
			train_wait( &tr, &rpReply );
		}

	}

	// Train task never exits
	assert( 1 == 0 );
}

void train_init ( Train *tr ) {

	int shellTid;
	TrainInit init;
	
	// Get the train number from shell
	Receive ( &shellTid, (char*)&init, sizeof(TrainInit) );
	
	// Copy the data to the train
	tr->id   		= init.id;
	tr->defaultGear = init.gear;

	// Reply to the shell
	Reply   ( shellTid, (char*)&tr->id, sizeof(int) );

	// The train talks to several servers
	tr->rpTid = WhoIs (ROUTEPLANNER_NAME);
	tr->tsTid = WhoIs (TRACK_SERVER_NAME);
	tr->deTid = WhoIs (TRACK_DETECTIVE_NAME);
	tr->csTid = WhoIs (CLOCK_NAME);
	assert( tr->rpTid >= NO_ERROR );
	assert( tr->tsTid >= NO_ERROR );
	assert( tr->deTid >= NO_ERROR );
	assert( tr->csTid >= NO_ERROR );
	
	// Initialize internal variables
	tr->mode = INIT_LOC;
	tr->currSensor = 0;
	tr->prevSensor = 0;

	// Initialize the calibration data
	rb_init( &(tr->hist), tr->histBuf );
	memoryset( (char *) tr->histBuf, 0, sizeof(tr->histBuf) );
	tr->velocity.mm = 0;
	tr->velocity.ms = 0;
	tr->ticks = Time( tr->csTid );
	tr->speedChanged = false;
	tr->cal_loops = 0;

	// Create a watchman
	tr->waTid = Create( WATCHMAN_PRIORITY, &watchman ); 
	
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
	tr->gear  = tr->defaultGear;

	// If you should reverse / are backwards, turn around.
	if( rep->stopDist < 0 ) {

		train_reverse( tr );			// TODO this
		rep->stopDist = -rep->stopDist;

	} else if( rep->stopDist == 0 ) { 

		tr->speedChanged = true;

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
			default:
				debug( "train: is stopping and idle now \r\n" );
				tr->mode = IDLE;
				tr->gear = 0;
				break;
		}
	}
	debug ("train: %d DRIVING AT gear=%d\r\n", tr->id, tr->gear);
	train_drive( tr->tsTid, tr->id, tr->gear);
}


void train_update( Train *tr, DeReply *rpl ) {
	int 		last, avg, diff, dist;
	Speed		sp, pr;

	// Update train location
	tr->prevSensor = tr->currSensor;
	tr->currSensor = rpl->sensor; 

	// Calculate the distance traveled
	dist = train_distance( tr );
	
	sp.ms = (rpl->ticks - tr->ticks) * MS_IN_TICK;
	sp.mm = dist;
	last = speed( sp );
	
	// Calculate the mean speed
	pr = train_speed( tr );
	avg = speed( pr );
	
	// Update the mean speed
	if( ! tr->speedChanged ) {
		debug( "adding speed %d to history data\r\n");
		rb_force( &tr->hist, &sp );
	}
	tr->speedChanged = false;
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

		
	tr->ticks = rpl->ticks;
	tr->velocity = sp;

}

//-----------------------------------------------------------------------------
//-------------------------- Shell Commands -----------------------------------
//-----------------------------------------------------------------------------

void train_watch ( Train *tr, RPReply *rep ) {
	// Disaster prevention
	Disaster	disaster;

	disaster.ticks 		= tr->ticks;
	disaster.id			= tr->id;
	disaster.mode		= tr->mode;
	disaster.sensor 	= tr->currSensor;
	disaster.crashDist 	= rep->stopDist;
	disaster.minDist	= 340; // TODO this should be calibrated
	disaster.velocity 	= tr->velocity;
	disaster.stopAction = rep->stopAction;
	disaster.gear		= tr->gear;

	// Tell the watchman
	Send( tr->waTid, (char *) &disaster, sizeof( Disaster ), 0, 0 );

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
	tr->gear		= tr->defaultGear;
	tr->mode 		= cmd.mode;

	debug ("Train %d is at sensor %d going to destidx %d\r\n",
			tr->id, tr->currSensor, tr->dest);

	// Reply to the shell
	Reply   ( shellTid, (char*)&tr->id, sizeof(int) );
}


//-----------------------------------------------------------------------------
//-------------------------- Route Planner Commands ---------------------------
//-----------------------------------------------------------------------------

int train_distance( Train *tr ) {
	RPRequest	req;
	int	dist;
	
	req.type		= MIN_SENSOR_DIST;
	req.sensor1		= tr->prevSensor;
	req.sensor2		= tr->currSensor;

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
	req.lastSensor	= tr->currSensor;
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

	train_drive( tr->tsTid, tr->id, INIT_GEAR );

	FOREVER {
		req.expire = timeout + Time( tr->csTid );
		//eprintf( "Train is lost, trying to find itself until %d.\r\n", req.expire );
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

void train_wait( Train *tr, RPReply *rep ) {
	DeReply 	rpl;
	DeRequest	req;
	int 		i;

	req.type      = WATCH_FOR;
	req.numEvents = rep->nextSensors.len;

	assert( req.numEvents < array_size( req.events ) );
	for( i = 0; i < req.numEvents ; i ++ ) {
		req.events[i].sensor = rep->nextSensors.idxs[i];
		req.events[i].start  = tr->ticks;
	//		+ speed_time ( tr->velocity, rep->nextSensors.dists[i] ) 
	//		- (PREDICTION_WINDOW / 2);
		req.events[i].end    = req.events[i].start + PREDICTION_WINDOW;
		
	}

	req.expire = tr->ticks 
					+ 200
					+ speed_time( tr->velocity, rep->stopDist ) / MS_IN_TICK;

	// TODO: HACK 
	// Ideally, we can predict the speed when starting.
	if ( tr->speedChanged == 1 ) {
		req.expire += INIT_GRACE;

	}

	// TODO do we need this?
	if( tr->mode == INIT_LOC ) {
		req.expire += INIT_GRACE;
	}

	// Tell the detective about the Route Planner's prediction.
	Send( tr->deTid, (char *) &req, sizeof(DeRequest),
					 (char *) &rpl, sizeof(TSReply) );

	// Did not predict properly
	if( rpl.sensor < NO_ERROR ) {
		//eprintf( "Train Missed a sensor.\r\n" );

		assert( Time( tr->csTid ) > req.expire );

		train_locate( tr );

	} else {
		train_update( tr, &rpl );
	}

}

//-----------------------------------------------------------------------------
//-------------------------- Track Server Commands ----------------------------
//-----------------------------------------------------------------------------
void train_reverse( Train *tr ) {
	TSRequest req;
	TSReply	  reply;
	
	if( tr->mode == CAL_SPEED ) return;

	tr->speedChanged = true;

	req.type = RV;
	req.train = tr->id;

	Send( tr->tsTid, (char *)&req,   sizeof(TSRequest),
					 (char *)&reply, sizeof(TSReply) );
}

void train_drive( TID tsTid, int trainId, int gear ) {
	debug ("Train %d is driving at gear =%d\r\n", trainId, gear);
	TSRequest req;
	TSReply	  reply;

	//if( tr->mode == CAL_SPEED ) return;

	req.type = TR;
	req.train = trainId;
	req.speed = gear;

	Send( tsTid, (char *)&req,   sizeof(TSRequest),
				 (char *)&reply, sizeof(TSReply) );
}

void train_flipSwitches( Train *tr, RPReply *rpReply ) {
	TSRequest		req;
	TSReply			reply;
	SwitchSetting  *ss;
	int				err;

	if( tr->mode != NORMAL && tr->mode != INIT_LOC ) return;

	foreach( ss, rpReply->switches ) {
		if( ss->id > 0 ) {
			debug("train: flipping sw%d to %c\r\n",
					ss->id, switch_dir(ss->dir));
			req.type = SW;
			req.sw   = ss->id;
			req.dir  = ss->dir;

			err = Send( tr->tsTid, (char *)&req,   sizeof(TSRequest),
									 (char *)&reply, sizeof(TSReply) );
			if( err < NO_ERROR ) {
				//eprintf( "Track Server did not flip switch. (%d)\r\n", err );
			}
		} else {
			break;
		}
	}
}

SwitchDir train_dir( Train *tr, int sw ) {
	TSRequest 	req = { ST, {sw}, {0} };
	TSReply		reply;

	Send( tr->tsTid, (char *)&req,	  sizeof(TSRequest), 
					  (char *)&reply, sizeof(TSReply) );
	return reply.dir;
}


//-----------------------------------------------------------------------------
//--------------------------------- Watchman ----------------------------------
//-----------------------------------------------------------------------------
//-- Question: Who watches the watchman? --------------------------------------

void heart() {
	TID parent	= MyParentTid();
	TID csTid 	= WhoIs( CLOCK_NAME );
	int	ticks;
	
	FOREVER {
		ticks = Delay( HEARTBEAT, csTid );
		// Heart beat to the watchman
		Send( parent, (char *) &ticks, sizeof(int), 0, 0 );
	}
}

void watchman( ) {
	debug( "who watches the watchman?\r\n" );

	Disaster 	disaster = { 0, 0, INIT_LOC, JUST_STOP, 0, 0, 0, 0, {0, 0} };
	TID			tid;
	TID			tsTid = WhoIs( TRACK_SERVER_NAME );
	int			len;
	int			distance = 0;
	int			ticks = 0;
	int			timeToReverse = 0;
	bool		stopped = false;
	bool		reversed = false;
	TID			uiTid = WhoIs( UI_NAME );

	UIRequest 	uiReq;
	uiReq.type = TRAIN;

	Create( TRAIN_PRTY - 1, &heart ); // watchman has a heart - awwwwwww

	FOREVER {
		// Wait until we learn about a disaster
		len = Receive( &tid, (char *) &disaster, sizeof( Disaster ) );
		Reply ( tid, 0, 0 );

		if( len == sizeof( int ) ) { //heartbeat
			distance = speed_dist( disaster.velocity, 
					(disaster.ticks - ticks) * MS_IN_TICK );
			
			// Send this information to the UI
			uiReq.idx  = disaster.sensor;
			uiReq.dist = distance;

			Send( uiTid, (char*)&uiReq, sizeof(UIRequest), 
						 (char*)&distance, sizeof(int) ); // filler ...

			if( disaster.mode == NORMAL ) {
				// Stop the train if needed
				if( (disaster.crashDist - distance) < disaster.minDist ) {
					if ( !stopped ) {
	//	printf ( "\033[41m wathchman stopping %dmm \033[49m\r\n", 
//						disaster.crashDist - distance );
						// Reverse the train if needed
						train_drive( tsTid, disaster.id, 0 ); 

						// prevent the watchman from being too heroic 
						stopped = true;
						timeToReverse = isqrt( speed(disaster.velocity) * C ) 
										+ disaster.ticks;
					}
				}

				
				// The train should be stopped, reverse it
				if( stopped && !reversed && (timeToReverse < disaster.ticks)
						&& disaster.stopAction == STOP_AND_REVERSE ) {
			printf ( "\033[41m wathchman reversing \033[49m\r\n" );
					train_drive( tsTid, disaster.id, 15 );
					train_drive( tsTid, disaster.id, disaster.gear );
					reversed = true;
				}
			}

		} else {	// update disaster
			assert( len == sizeof( Disaster ));
			ticks = disaster.ticks;
			distance = 0;
			reversed = false;
			stopped = false;
			assert( disaster.crashDist >= 0 );
			debug( "watchman: got disaster update\r\n" );
		}
	}
}


