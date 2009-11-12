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
#include "servers.h"
#include "train.h"

// Private Stuff
// ----------------------------------------------------------------------------

typedef struct {
	int  id;			// Identifying number
	int  speed;			// The current speed
	int  currNode;		// Current Location
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

void train_init    (Train *t);
int  train_getTrvlDist (Train *t, RPRequest *req);
void train_reverse (Train *t);
void train_drive   (Train *t, int speed);
// ----------------------------------------------------------------------------


void train_run () {
	int shellTid;
	TrainInit initStuff;
	Train tr;
	RPRequest rpReq;
	Node *prevNode;

	// Initialize this train
	train_init ( &tr );
	rpReq.trainId = tr.id;
	
		// Until you've reached your destination:
	while ( currNode != dest ) {
	
		// talk to route planner
		// set speed to start on route
		// make prediction
		// verify prediction
		// calibrate speed profile


	}
	Exit(); // When you've reached your destination
}

void train_init ( Train *t ) {
	
	// Get the train number from shell
	// determine starting location from shell. Set as current location 
	Receive ( &shellTid, (char*)&initStuff, sizeof(TrainInit) );
	Reply   ( shellTid, (char*)&initStuff.id, sizeof(int) );

	// determine destination from shell
	//
	tr->rpTid = WhoIs (ROUTEPLANNER_NAME);

	// TODO: Does this work?
	RegisterAs ("Train" + tr->id);
}

int train_getTrvlDist (Train *t, RPRequest *req) {
	RPReply   rpReply;
	
	rpReq.type = PLANROUTE;
	rpReq.idx1 = tr.currNode;
	rpReq.idx2 = prevNode;


	Send (t->rpTid, (char*) &rpReq, sizeof(RPReq),
			 		(char*) &rpReply, sizeof(RPReply));
	
	if ( rpReply.dist < 0 ) {
		train_reverse(t);
	}
}

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

needs to:
talk to route planner to plan route from currNode -> dest
talk to route planner to reserve some track
talk to detective to predict which track node it will hit next
talk to trackserver to set its speed & flip switches
calibrate its speed
talk to detective / track controller to ???
