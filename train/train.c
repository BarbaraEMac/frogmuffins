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

#define	SPEED_HIST				10
#define PREDICTION_WINDOW 		(1000 /MS_IN_TICK)
#define	BLANK_RECORD_DIVISOR	2
#define	HEARTBEAT				(200 /MS_IN_TICK)
#define WATCHMAN_PRIORITY		3

// Private Stuff
// ----------------------------------------------------------------------------

typedef struct {
	int 		mm;
	int 		ms;
} Speed;

typedef struct {
	int			ticks;			// used by the heartbeat
	int			id;				// Identifyin train number
	int			sensor;			// last landmark
	int			crashDist;		// in mm
	int			minDist;		// in mm
	Speed		velocity;		// The actual current speed (in mm/s)

} Disaster;

typedef struct {
	int  		id;				// Identifying number
	int  		currSensor;		// Current Location
	int	 		prevSensor;		// Previous Location
	int  		dest;			// Desired End Location
	TrainMode	mode;			// Train Mode (calibration or normal)
		
	int 	    speedSet;		// The current speed setting
	bool		speedChanged;	// Has the speed changed
	Speed		velocity;		// The actual current speed (in mm/s)
	
	TID 		rpTid;			// Id of the Route Planner
	TID 		tsTid;			// Id of the Track Server
	TID			deTid;			// Id of the Detective
	TID			csTid;			// Id of the Clock Server
	TID			waTid;			// Id of the Watchman

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
void 		train_predictSpeed	( Train *tr, int mm );

// Route Planner Commands
RPReply		train_planRoute 	( Train *tr );
int 		train_distance		( Train *tr ); // in mm

// Detective Commands
void	 	train_wait			( Train *tr, RPReply *rep );
void 		train_update		( Train *tr, DeReply *rpl );

// Track Server Commands
void 		train_reverse		( Train *tr );
void 		train_drive  		( Train *tr, int speedSet );
void		train_flipSwitches	( Train *tr, RPReply *rpReply );
SwitchDir	train_dir			( Train *tr, int sw );

// Watchman
void		watchman			();
// ----------------------------------------------------------------------------

void train_run () {
	Train 	  tr;
	RPReply   rpReply;		// Reply from the Route Planner

	// Initialize this train
	train_init ( &tr );
	
	// Until you've reached your destination:
	while ( tr.currSensor != tr.dest ) {
		// Ask the Route Planner for an efficient route!
		rpReply = train_planRoute ( &tr );
		debug ("train: has a route stopDist=%d\r\n", rpReply.stopDist);

		if_error( rpReply.err, "Route Planner failed" );
		if( rpReply.err < NO_ERROR ) {
			rpReply.stopDist = 500; // TODO this is a hack to be replaced with locate

			rpReply.switches[0].id = -1;
			rpReply.nextSensors.len = 0;
		}
		
		// If you should reverse / are backwards, turn around.
		if( rpReply.stopDist < 0 ) {
			train_reverse(&tr);
			rpReply.stopDist = -rpReply.stopDist;
		}
		
		// Flip the switches along the path.
		train_flipSwitches (&tr, &rpReply);

	assert( tr.deTid == 9 );
		// Set the speed to an appropriate setting
		train_predictSpeed (&tr, rpReply.stopDist);

	assert( tr.deTid == 9 );
		// Tell the detective about your predicted sensors
		train_wait(&tr, &rpReply);
	}

	Exit(); // When you've reached your destination
}

void train_init ( Train *tr ) {
	int shellTid;
	TrainInit init;
	
	// Get the train number from shell
	Receive ( &shellTid, (char*)&init, sizeof(TrainInit) );

	// Copy the data to the train
	tr->id      	= init.id;
	tr->currSensor 	= init.currLoc;
	tr->dest   		= init.dest;
	tr->speedSet	= 6;
	tr->mode 		= init.mode;

	debug ("Train %d is at sensor %d going to destidx %d\r\n",
			tr->id, tr->currSensor, tr->dest);

	// Reply to the shell
	Reply   ( shellTid, (char*)&tr->id, sizeof(int) );

	// determine destination from shell
	tr->rpTid = WhoIs (ROUTEPLANNER_NAME);
	tr->tsTid = WhoIs (TRACK_SERVER_NAME);
	tr->deTid = WhoIs (TRACK_DETECTIVE_NAME);
	tr->csTid = WhoIs (CLOCK_NAME);
	assert( tr->rpTid >= NO_ERROR );
	assert( tr->tsTid >= NO_ERROR );
	assert( tr->deTid >= NO_ERROR );
	assert( tr->csTid >= NO_ERROR );
	
	// Initialize the calibration data
	rb_init( &(tr->hist), tr->histBuf );
	memoryset( (char *) tr->histBuf, 0, sizeof(tr->histBuf) );
	tr->velocity.mm = 0;
	tr->velocity.ms = 0;
	tr->ticks = Time( tr->csTid );
	tr->speedChanged = false;

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
	return total;
}


void train_predictSpeed( Train *tr, int mm ) {

	//Speed current = train_speed( tr );
	// TODO: PLACE calibration here
	//tr->speedSet  = 6;
	if( mm < 10 ) { 

		tr->speedChanged = true;

		tr->speedSet = 0;
		train_drive (tr, 0);
		debug("train: Reached it's destination.");
		Exit ();
	}
	//train_drive(tr, tr->speedSet);

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
	pr = train_speed( tr );
	avg = speed( pr );
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
//-------------------------- Route Planner Commands ---------------------------
//-----------------------------------------------------------------------------

int train_distance( Train *tr ) {
	RPRequest	req;
	int	dist;
	
	req.type		= MIN_SENSOR_DIST;
	req.sensor1		= tr->prevSensor;
	req.sensor2		= tr->currSensor;

	assert( tr->rpTid == 12 );
	Send( tr->rpTid, (char*) &req,  sizeof(RPRequest),
			 		 (char*) &dist, sizeof(int) );
	
	if_error( dist, "train: distance returned an error" );
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

	assert( tr->rpTid == 12 );
	Send( tr->rpTid, (char*) &req,   sizeof(RPRequest),
			 		 (char*) &reply, sizeof(RPReply) );
	
	return reply;
}

//-----------------------------------------------------------------------------
//-------------------------- Detective Commands -------------------------------
//-----------------------------------------------------------------------------

void train_wait( Train *tr, RPReply *rep ) {
	DeReply 	rpl;
	DeRequest	req;
	int 		i;

	assert( tr->deTid == 9 );
	req.type      = WATCH_FOR;
	req.numEvents = rep->nextSensors.len;

	assert( req.numEvents < array_size( req.events ) );
	for( i = 0; i < req.numEvents ; i ++ ) {
		req.events[i].sensor = rep->nextSensors.idxs[i];
		req.events[i].start  = tr->ticks;//train_time ( tr->velocity, tr ) - (PREDICTION_WINDOW/2);
		req.events[i].end    = req.events[i].start + PREDICTION_WINDOW;
		
	}

	req.expire = tr->ticks 
					+ 1000
					+ speed_time( tr->velocity, rep->stopDist ) / MS_IN_TICK;

	assert( tr->deTid == 9 );
	// Tell the detective about the Route Planner's prediction.
	Send( tr->deTid, (char *) &req, sizeof(DeRequest),
					 (char *) &rpl, sizeof(TSReply) );

	// Did not predict properly
	if( rpl.sensor < NO_ERROR ) {
		error( rpl.sensor, "Train Missed a sensor." );

		assert( Time( tr->csTid ) > req.expire );
		// Reverse the train if needed
		if( rep->stopAction == STOP_AND_REVERSE ) {
			train_reverse( tr );
		}

		req.type = GET_STRAY;
		// Try again
		Send( tr->deTid, (char *) &req, sizeof(DeRequest),
						 (char *) &rpl, sizeof(TSReply) );
	}

	train_update( tr, &rpl );

	Disaster	disaster;

	disaster.ticks 		= tr->ticks;
	disaster.id			= tr->id;
	disaster.sensor 	= tr->currSensor;
	disaster.crashDist 	= rep->stopDist;
	disaster.minDist	= 300; // TODO this should be calibrated
	disaster.velocity 	= tr->velocity;
	Send( tr->waTid, (char *) &disaster, sizeof( Disaster ), 0, 0 );

}

//-----------------------------------------------------------------------------
//-------------------------- Track Server Commands ----------------------------
//-----------------------------------------------------------------------------
void train_reverse( Train *tr ) {
	TSRequest req;
	TSReply	  reply;
	
	if( tr->mode == CALIBRATION ) return;

	tr->speedChanged = true;

	req.type = RV;
	req.train = tr->id;

	assert( tr->tsTid == 8 );
	Send( tr->tsTid, (char *)&req,   sizeof(TSRequest),
					 (char *)&reply, sizeof(TSReply) );
}

void train_drive( Train *tr, int speedSet ) {
	TSRequest req;
	TSReply	  reply;

	req.type = TR;
	req.train = tr->id;
	req.speed = speedSet;

	assert( tr->tsTid == 8 );
	Send( tr->tsTid, (char *)&req,   sizeof(TSRequest),
					 (char *)&reply, sizeof(TSReply) );
}

void train_flipSwitches( Train *tr, RPReply *rpReply ) {
	TSRequest		req;
	TSReply			reply;
	SwitchSetting  *ss;

	if( tr->mode == CALIBRATION ) return;

	foreach( ss, rpReply->switches ) {
		if( ss->id > 0 ) {
			debug("train: flipping sw%d to %c\r\n",
					ss->id, switch_dir(ss->dir));
			req.type = SW;
			req.sw   = ss->id;
			req.dir  = ss->dir;

			assert( tr->deTid == 9 );
			assert( tr->tsTid == 8 );
			Send( tr->tsTid, (char *)&req,   sizeof(TSRequest),
							 (char *)&reply, sizeof(TSReply) );
			if_error( reply.ret, "Track Server did not flip switch.");
		} else {
			break;
		}
	}
}

SwitchDir train_dir( Train *tr, int sw ) {
	TSRequest 	req = { ST, {sw}, {0} };
	TSReply		reply;

	assert( tr->tsTid == 8 );
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

	Disaster 	disaster = { 0, 0, 0, 0, 0, {0, 0} };
	TID			tid;
	int			len;
	int			distance = 0;
	int			ticks = 0;
	
	Create( 3, &heart ); // watchman has a heart - awwwwwww

	FOREVER {
		// Wait until we learn about a disaster
		len = Receive( &tid, (char *) &disaster, sizeof( Disaster ) );
		Reply ( tid, 0, 0 );

		if( len == sizeof( int ) ) { //heartbeat
			distance = speed_dist( disaster.velocity, disaster.ticks - ticks );
			// TODO replace with a call to UI
			printf( "\033[10;30H%c%d:%dmm\033[24;0H", 
					sensor_bank( disaster.sensor ),
					sensor_num( disaster.sensor ), ticks );
			// Stop the train if needed
			if( (disaster.crashDist - distance) < disaster.minDist ) {
				error ( TIMEOUT, "EMERGENCY TRAIN STOP" );
			}

		} else {	// update disaster
			assert( len == sizeof( Disaster ));
			ticks = disaster.ticks;
			distance = 0;
			debug( "watchmen: got disaster update" );
		}
	}
}


