/**
* CS 452: User Task
* becmacdo
* dgoc
*/
#define DEBUG 2

#include <string.h>
#include <ts7200.h>

#include "debug.h"
#include "error.h"
#include "requests.h"
#include "routeplanner.h"
//#include "servers.h"
#include "trackserver.h"
#include "train.h"

// Private Stuff
// ----------------------------------------------------------------------------

typedef struct {
	int  id;			// Identifying number
	int  currLoc;		// Current Location
	int	 prevLoc;		// Previous Location
	int  dest;			// Desired End Location
	
	int  speed;			// The current speed
	
	int rpTid;			// Id of the Route Planner
	int tsTid;			// Id of the Track Server
	int	deTid;			// Id of the Detective
} Train;



void 	train_init    			(Train *t, RPRequest *rpReq);

// Route Planner Commands
RPReply	train_planRoute 		(Train *t, RPRequest *req);

// Detective Commands
TSReply train_makePrediction	(Train *tr, RPReply *rep);

// Track Server Commands
void 	train_reverse 			(Train *t);
void 	train_drive   			(Train *t, int speed);
void	train_flipSwitch		(Train *t, RPReply *rpReply);
// ----------------------------------------------------------------------------

void train_run () {
	Train 	  tr;
	RPRequest rpReq;		// Route Planner request
	RPReply   rpReply;		// Reply from the Route Planner
	TSReply	  tsReply;		// Reply from the Detective
	int someDistanceTravelled = 0;

	// Initialize this train
	train_init ( &tr, &rpReq );
	
	// Until you've reached your destination:
	while ( tr.currLoc != tr.dest ) {
		// Ask the Route Planner for an efficient route!
		rpReply = train_planRoute (&tr, &rpReq);
		debug ("train: has a route\r\n");
		debug ("Next node=%d checkinDist=%d reverse?=%d sw?=%d dir=%d\r\n", rpReply.path.path[0], rpReply.checkinDist, rpReply.reverse, rpReply.switchId, rpReply.switchDir);

		// This makes the assumption that yo uare NOWHERE
		// near a switch. Neither forward nor backwards can be a sw.
		// If you are backwards, turn around.
		if ( rpReply.path.path[0] == tr.prevLoc ) {
			train_reverse(&tr);
		}

		// TODO: Will this call block until we've reached our sensor?
		// It probably shouldn't so that we can calibrate ..
		// Tell the detective about your predicted sensors
//		tsReply = train_makePrediction(&tr, &rpReply);

		// TODO: This is just to start ....
		// Drive until you need to check in
		// Start driving slowly ... 
		debug ("train is driving\r\n");
		train_drive (&tr, 6);
		while ( someDistanceTravelled < rpReply.checkinDist ) {
			someDistanceTravelled ++;
			; // BUSY WAIT
		}
		// Stop for now ...
		train_drive (&tr, 0);
		debug ("train is stopping\r\n");
		
		// If you should reverse, do it.
		if ( rpReply.reverse == 1 ) {
			train_reverse(&tr);
		}
		// If you should flip a switch, do it.
		if ( rpReply.switchId != -1 ) {
			train_flipSwitch (&tr, &rpReply);
		}

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
	
	// Initialize the Route Planner request
	rpReq->trainId = tr->id;
	rpReq->dest    = tr->dest;

	// TODO: Does this work?
	RegisterAs ("Train");// + tr->id);
}

RPReply train_planRoute (Train *tr, RPRequest *req) {
	RPReply reply;
	
	req->type = PLANROUTE;
	req->idx1 = tr->currLoc;
	req->idx2 = tr->prevLoc;
	req->dest = tr->dest;

	Send (tr->rpTid, (char*)  req,   sizeof(RPRequest),
			 		 (char*) &reply, sizeof(RPReply));
	
	return reply;
}

TSReply train_makePrediction (Train *tr, RPReply *rep) {
	TSReply reply;

	// Tell the detective about the Route Planner's prediction.
	Send ( tr->deTid, (char*)&rep->prediction, sizeof(Path), 
					  (char*)&Reply, sizeof(TSReply) );
	return reply;
}

//-----------------------------------------------------------------------------
//-------------------------- Track Server Commands ----------------------------
//-----------------------------------------------------------------------------
void train_reverse (Train *t) {
	TSRequest req;
	TSReply	  reply;

	req.type = RV;
	req.train = t->id;

	Send (t->tsTid, (char*)&req,   sizeof(TSRequest),
					(char*)&reply, sizeof(TSReply));
}

void train_drive (Train *t, int speed) {
	TSRequest req;
	TSReply	  reply;

	req.type = TR;
	req.train = t->id;
	req.speed = speed;

	Send (t->tsTid, (char*)&req,   sizeof(TSRequest),
					(char*)&reply, sizeof(TSReply));
}

void train_flipSwitch (Train *tr, RPReply *rpReply) {
	TSRequest req;
	TSReply	  reply;

	req.type = SW;
	req.sw   = rpReply->switchId;
	req.dir  = rpReply->switchDir;

	Send (tr->tsTid, (char*)&req,   sizeof(TSRequest),
			  		 (char*)&reply, sizeof(TSReply));
}
