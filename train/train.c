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

#define	SPEED_HIST				20
#define PREDICTION_WINDOW 		1000

// Private Stuff
// ----------------------------------------------------------------------------

typedef struct {
	int 		mm;
	int 		ms;
} Speed;

typedef struct {
	int  		id;			// Identifying number
	int  		currLoc;		// Current Location
	int	 		prevLoc;		// Previous Location
	int  		dest;			// Desired End Location
		
	int 		 speed;			// The current speed
	
	int 		rpTid;			// Id of the Route Planner
	int 		tsTid;			// Id of the Track Server
	int			deTid;			// Id of the Detective

	int			dist;
	int			ticks;
	int 		numSw;
	Speed		histBuf[SPEED_HIST];
	RB			hist;	
	TrackModel	model;
	int			sensor; 		//TODO this is temporary

} Train;

int speed( Speed *sp ) {
	if( sp->ms == 0 ) sp->ms = -1;
	return (sp->mm * 1000) / sp->ms;
}


void 		train_init    		(Train *tr, RPRequest *rpReq);
int			train_time			(Train *tr, int mm);
Speed 		train_speed			(Train *tr);

// Route Planner Commands
RPReply		train_planRoute 	(Train *tr, RPRequest *req);

// Detective Commands
void	 	train_wait			(Train *tr, RPReply *rep);
void 		train_update		(Train *tr, DeReply *rpl);

// Track Server Commands
void 		train_reverse		(Train *tr);
void 		train_drive  		(Train *tr, int speed);
void		train_flipSwitches	(Train *tr, RPReply *rpReply);
SwitchDir	train_dir			(Train *tr, int sw );
// ----------------------------------------------------------------------------

void train_run () {
	Train 	  tr;
	RPRequest rpReq;		// Route Planner request
	RPReply   rpReply;		// Reply from the Route Planner
	int someDistanceTravelled = 0;

	// Initialize this train
	train_init ( &tr, &rpReq );
	
	// Until you've reached your destination:
	while ( tr.currLoc != tr.dest ) {
		// Ask the Route Planner for an efficient route!
		rpReply = train_planRoute (&tr, &rpReq);
		debug ("train: has a route\r\n");

		// This makes the assumption that yo uare NOWHERE
		// near a switch. Neither forward nor backwards can be a sw.
		// If you are backwards, turn around.
		// TODO

		// TODO: This is just to start ....
		// Drive until you need to check in
		// Start driving slowly ... 
		debug ("train is driving\r\n");
		train_drive (&tr, 6);

		// It probably shouldn't so that we can calibrate ..
		// Tell the detective about your predicted sensors
		train_wait(&tr, &rpReply);

		while ( someDistanceTravelled < rpReply.stopDist ) {
			someDistanceTravelled ++;
			; // BUSY WAIT
		}
		// Stop for now ...
		//train_drive (&tr, 0);
		//debug ("train is stopping\r\n");
		
		// If you should reverse, do it.
		if( rpReply.stopDist < 0 ) {
			train_reverse(&tr);
		}
		// If you should flip a switch, do it.
		train_flipSwitches (&tr, &rpReply);

	}

	Exit(); // When you've reached your destination
}

void train_init ( Train *tr, RPRequest *rpReq ) {
	int shellTid;
	TrainInit init;
	
	// Get the train number from shell
	Receive ( &shellTid, (char*)&init, sizeof(TrainInit) );

	// Copy the data to the train
	tr->id      = init.id;
	tr->currLoc = init.currLoc;
	tr->prevLoc = init.prevLoc;
	tr->dest    = init.dest;

	// Reply to the shell
	Reply   ( shellTid, (char*)&tr->id, sizeof(int) );

	// determine destination from shell
	tr->rpTid = WhoIs (ROUTEPLANNER_NAME);
	tr->tsTid = WhoIs (TRACK_SERVER_NAME);
	tr->deTid = WhoIs (TRACK_DETECTIVE_NAME);
	assert( tr->deTid >= NO_ERROR );
	
	// Initialize the Route Planner request
	rpReq->trainId = tr->id;
	rpReq->destIdx = tr->dest;

	// Initialize the calibration data
	tr->numSw = 0;
	parse_model( TRACK_B, &tr->model );
	rb_init( &(tr->hist), tr->histBuf );
	memoryset( (char *) tr->histBuf, 0, sizeof(tr->histBuf) );

//	RegisterAs ("Train");;
}

Speed train_speed( Train *tr ) {
	Speed total = {0, 0}, *rec;

	foreach( rec, tr->histBuf ) {
		total.mm += rec->mm;
		total.ms += rec->ms;
		debug("buffer %d/%d = \t%dmm/s\r\n",
			rec->mm, rec->ms, speed( rec ));
	}
	return total;
}

int	train_time (Train *tr, int mm) {
	Speed avg = train_speed( tr );

	return (avg.ms * mm) / avg.mm;
}

int tr_distance( Train *tr, int sensor ) {
	debug("tr_distance: sensor:%d \r\n", sensor );
	int idx = tr->model.sensor_nodes[sensor];

	Node *n = &tr->model.nodes[idx];
	Edge *e;
	int dist = 0;
	SwitchDir dir = SWITCH_CURVED;
	if( sensor % 2 ) {
		e = &n->se.behind;
	} else {
		e = &n->se.ahead;
	}

	tr->numSw = 0;
	while( 1 ) {
		dist += e->distance;
		printf("(%d)%s>", idx, n->name);
		n = &tr->model.nodes[e->dest];
		//printf("neighbour %s\r\n", n->name);
		if( n->type != NODE_SWITCH ) break;
		dir = train_dir( tr, n->id);
		if( n->sw.behind.dest == idx ) {
			idx = e->dest;
			e = &n->sw.ahead[dir];
		} else {
			idx = e->dest;
			e = &n->sw.behind;
		}
		tr->numSw++;
	} 

	sensor = n->id * 2;
	if( n->se.ahead.dest == idx ) 
		sensor ++;

	printf("(%d)%s\r\n", idx, n->name );
	return dist;
}

void train_update( Train *tr, DeReply *rpl ) {
	int 		last, avg, diff;
	Speed		sp, pr;
	
	sp.ms = (rpl->ticks - tr->ticks) * MS_IN_TICK;
	sp.mm = tr->dist;
	last = speed( &sp );
	pr = train_speed( tr );
	avg = speed(&pr);
	rb_force( &tr->hist, &sp );
	diff = ((avg * sp.ms) - (sp.mm * 1000)) / 1000;

	if( abs(diff) > 100 )
		printf("\033[31m");
	else if( abs(diff) > 50 )
		printf("\033[33m");
	else if( abs(diff) > 20 )
		printf("\033[36m");
	else 
		printf("\033[32m");
	printf("speed = %d/%d = \t%dmm/s \tavg:%dmm/s \tdiff:%dmm\033[37m\r\n",
			sp.mm, sp.ms, last, avg, diff);

	// Update train location
	tr->prevLoc = tr->currLoc;
	tr->currLoc = rpl->sensor / 2; // TODO fix this
	
	tr->dist = tr_distance( tr, rpl->sensor );
	tr->ticks = rpl->ticks;

}

RPReply train_planRoute (Train *tr, RPRequest *req) {
	RPReply reply;
	
	req->type = PLANROUTE;
	req->lastSensor = tr->currLoc;
	req->destIdx = tr->dest;

	Send (tr->rpTid, (char*)  req,   sizeof(RPRequest),
			 		 (char*) &reply, sizeof(RPReply));
	
	return reply;
}

void train_wait (Train *tr, RPReply *rep) {
	TSReply 	rpl;
	DeRequest	req;
	int 		i;

	req.type = WATCH_FOR;
	req.numEvents = rep->nextSensors.len;

	for( i = 0; i < req.numEvents ; i ++ ) {
		req.events[i].sensor = rep->nextSensors.idxs[i];
		req.events[i].start  = 0;//train_time ( tr ) - (PREDICTION_WINDOW/2);
		req.events[i].end    = req.events[i].start + PREDICTION_WINDOW;
printf("predicting %c%d, after %dms, before %dms\r\n",
	sensor_bank( req.events[i].sensor ), sensor_num( req.events[i].sensor ),
	req.events[i].start, req.events[i].end );
	}

	// Tell the detective about the Route Planner's prediction.
	Send( tr->deTid, (char *) &req, sizeof(DeRequest),
					 (char *) &rpl, sizeof(TSReply) );
}

//-----------------------------------------------------------------------------
//-------------------------- Track Server Commands ----------------------------
//-----------------------------------------------------------------------------
void train_reverse (Train *tr) {
	TSRequest req;
	TSReply	  reply;

	req.type = RV;
	req.train = tr->id;

	Send (tr->tsTid, (char *)&req,   sizeof(TSRequest),
					 (char *)&reply, sizeof(TSReply));
}

void train_drive (Train *tr, int speed) {
	TSRequest req;
	TSReply	  reply;

	req.type = TR;
	req.train = tr->id;
	req.speed = speed;

	Send (tr->tsTid, (char *)&req,   sizeof(TSRequest),
					 (char *)&reply, sizeof(TSReply));
}

void train_flipSwitches (Train *tr, RPReply *rpReply) {
	TSRequest		req;
	TSReply			reply;
	SwitchSetting  *ss;

	foreach( ss, rpReply->switches ) {
		if( ss->id >= 0 ) {
			req.type = SW;
			req.sw   = ss->id;
			req.dir  = ss->dir;

			Send (tr->tsTid, (char *)&req,   sizeof(TSRequest),
							 (char *)&reply, sizeof(TSReply));
		}
	}
}

SwitchDir train_dir( Train *tr, int sw ) {
	TSRequest 	req = { ST, {sw}, {0} };
	TSReply		reply;

	Send ( tr->tsTid, (char *)&req,	  sizeof(TSRequest), 
					  (char *)&reply, sizeof(TSReply));
	return reply.dir;
}


