	/**
	* CS 452: User Task
	* becmacdo
	* dgoc
	*/
#define DEBUG 1

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
	int  speed;			// The current speed
	int  currLoc;		// Current Location
	int	 prevLoc;		// Previous Location
	int  dest;			// Desired End Location
	
	int rpTid;			// Id of the Route Planner
	int tsTid;			// Id of the Track Server
	int	deTid;			// Id of the Detective
} Train;

typedef struct {
	int id;
	int behindIdx;
	int aheadIdx;
} TrainInit;

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

		// TODO: Will this call block until we've reached our sensor?
		// It probably shouldn't so that we can calibrate ..
		// Tell the detective about your predicted sensors
		tsReply = train_makePrediction(&tr, &rpReply);

		// If you should reverse, do it.
		if ( rpReply.totalDist < 0 ) {
			train_reverse(&tr);
		}
		// If you should flip a switch, do it.
		if ( rpReply.switchId != -1 ) {
			train_flipSwitch (&tr, &rpReply);
		}

		// TODO: This is just to start ....
		// Drive until you need to check in
		// Start driving slowly ... 
		train_drive (&tr, 5);
		while ( someDistanceTravelled < rpReply.checkinDist ) {
			; // BUSY WAIT
		}

		train_drive (&tr, 0);
	}

	Exit(); // When you've reached your destination
}

void train_init ( Train *tr, RPRequest *rpReq ) {
	int shellTid;
	TrainInit initStuff;	// TODO
	
	// Get the train number from shell
	// determine starting location from shell. Set as current location 
	Receive ( &shellTid, (char*)&initStuff, sizeof(TrainInit) );
	Reply   ( shellTid, (char*)&initStuff.id, sizeof(int) );

	// determine destination from shell
	tr->rpTid = WhoIs (ROUTEPLANNER_NAME);
	tr->tsTid = WhoIs (TRACK_SERVER_NAME);
	tr->deTid = WhoIs (TRACK_DETECTIVE_NAME);
	
	// Initialize the Route Planner request
	rpReq->trainId = tr->id;
	rpReq->dest    = tr->dest;

	// TODO: Does this work?
	RegisterAs ("Train" + tr->id);
}

RPReply train_planRoute (Train *tr, RPRequest *req) {
	RPReply reply;
	
	req->type = PLANROUTE;
	req->idx1 = tr->currLoc;
	req->idx2 = tr->prevLoc;

	Send (tr->rpTid, (char*) &req,   sizeof(RPRequest),
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
