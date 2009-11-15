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
#include "servers.h"
#include "train.h"

#define INT_MAX		0xFFFF
#define EPSILON		20

// Private Stuff
// ----------------------------------------------------------------------------

typedef struct {
	int		time1;		// Time period to reserve for
	int		time2;		// end time
	int 	dist; 		// Reservation distance in mm
	Node 	*start;		// Starting at this node
	Node 	*next1;		// Going towards this node
	Node	*next2;		// Only used for switches

} Reservation;

typedef struct {
	int 		dists[MAX_NUM_NODES][MAX_NUM_NODES];
	int			paths[MAX_NUM_NODES][MAX_NUM_NODES];

	TrackModel model;

	Reservation reserves[MAX_NUM_TRAINS];
} RoutePlanner;

inline SwitchDir getSwitchDir(Node *sw, Node *next);

int rp_init      		(RoutePlanner *rp);
int rp_errorCheckShRequest (RoutePlanner *rp, RPRequest *req);
int rp_errorCheckTrRequest (RoutePlanner *rp, RPRequest *req);
inline void clearReply (RPReply *trReply);

// Route Finding Functions
void rp_planRoute 		(RoutePlanner *rp, RPReply *trReply, RPRequest *req);
int  rp_turnAround 		(RoutePlanner *rp, Path *p, int sensorId);
int  rp_distToNextRv   	(RoutePlanner *rp, Path *p);
int  rp_distToNextSw   	(RoutePlanner *rp, Path *p);
void rp_getNextSwitchSettings (RoutePlanner *rp, Path *p, SwitchSetting *settings);

void rp_predictSensors  (RoutePlanner *rp, SensorsPred *p, int sensorId );
void rp_predictSensorHelper(RoutePlanner *rp, SensorsPred *p, Node *n, int prevIdx);
int  rp_minSensorDist 	(RoutePlanner *rp, int sensor1, int sensor2 );

// Reservation Functions
void rp_reserve   		(RoutePlanner *rp, RPRequest *req);
void cancelReserve		(TrackModel *model, Reservation *rsv);
int  makeReserve  		(TrackModel *model, Reservation *rsv);
int  mapTrainId   		(int trainId);

// Display to Monitor Functions
void rp_outputPath 	   	(RoutePlanner *rp, int i, int j);
void rp_displayFirstSw	(RoutePlanner *rp, RPRequest *req);
void rp_displayFirstRv 	(RoutePlanner *rp, RPRequest *req);
void rp_displayPath	  	(RoutePlanner *rp, Path *p );

// Shortest Path Algorithms
void floyd_warshall 	(RoutePlanner *rp, int n);
int  cost				(TrackModel *model, int idx1, int idx2);
void makePath 			(RoutePlanner *rp, Path *p, int i, int j);
void makePathHelper 	(RoutePlanner *rp, Path *p, int i, int j);

// Convert a sensor index into a node index
inline int sIdxToIdx ( int sIdx ) {
	return (int) (sIdx / 2);
}

inline int idxTosIdx (int idx, char *name) {
	int ret = idx * 2;
	if ( (atod(name[1]) % 2) == 0 ) {
		ret += 1;
	}
	debug ("idx=%d ret=%d\r\n", idx, ret);
	return ret;
}
// ----------------------------------------------------------------------------

// ahead[0] = straight
// ahead[1] = curved
inline SwitchDir getSwitchDir (Node *sw, Node *next) {
	Edge *e = &sw->sw.ahead[0];

	return ( e->dest == next->idx ) ? SWITCH_STRAIGHT : SWITCH_CURVED;
}

void rp_run() {
	debug ("rp_run\r\n");
	RoutePlanner	rp;
	int 			senderTid;
	RPRequest		req;
	RPReply			trReply;
	RPShellReply    shReply;
	int 			shellTid;
	SensorsPred		pred;	
	int 			i;
	int				err;
	int				tmp;
	char 			tmpStr[12];

	// Initialize the Route Planner
	shellTid = rp_init (&rp);

	FOREVER {
		// Receive from a client train
		Receive ( &senderTid, (char*)&req, sizeof(RPRequest) );
		debug("routeplanner: Rcvd sender=%d from=%d type=%d idx1=%d idx2=%d\r\n",
			  senderTid, req.trainId, req.type, req.nodeIdx1, req.nodeIdx2);

		// Error check the shell's request.
		if ( senderTid == shellTid ) {
			if( (err = rp_errorCheckShRequest(&rp, &req)) < NO_ERROR ) {
				req.type = ERROR;	// Make sure we don't do any work below.
				shReply.err = err;	// Return the error.
				Reply(senderTid, (char*)&shReply, sizeof(RPShellReply));
			} 
		// Error check a train's request.
		} else {
			if( (err = rp_errorCheckTrRequest(&rp, &req)) < NO_ERROR ) {
				req.type = ERROR;	// Make sure we don't do any work below.
				trReply.err = err;	// Return the error.
				Reply ( senderTid, (char*)&trReply, sizeof(RPReply) );
			}
		}

		switch ( req.type ) {
			case ERROR:
				// Do nothing since we handled the error already!
				break;

			case DISPLAYROUTE:
				// Reply to the shell
				Reply(senderTid, (char*)&shReply, sizeof(RPShellReply));

				// Display the path
				rp_outputPath ( &rp, req.nodeIdx1, req.nodeIdx2 );
				// Display the total distance
				printf ("%s\r\nDistance travelled = %d\r\n", 
						rp.model.nodes[req.nodeIdx2].name,
						rp.dists[req.nodeIdx1][req.nodeIdx2]);
				break;

			case DISPLAYFSTSW:
				// Reply to the shell
				Reply(senderTid, (char*)&shReply, sizeof(RPShellReply));
				
				rp_displayFirstSw (&rp, &req);

				break;

			case DISPLAYFSTRV:
				// Reply to the shell
				Reply(senderTid, (char*)&shReply, sizeof(RPShellReply));
				
				rp_displayFirstRv (&rp, &req);
				break;
			
			case DISPLAYPREDICT:
				// Reply to the shell
				Reply(senderTid, (char*)&shReply, sizeof(RPShellReply));
				
				pred.len = 0;

				rp_predictSensors(&rp, &pred, idxTosIdx(req.nodeIdx1, req.name));
				
				if ( req.nodeIdx1 > 39 ) {
					printf ("\033[36m%s is not a valid sensor.\033[37m\r\n", req.name);
				} else if ( pred.len == 0 ) {
					printf ("There are no sensors.\r\n");
				} else {
					printf ("Sensors: ");
					for ( i = 0; i < pred.len; i ++ ) {
						printf ("%c%d, ", sensor_bank(pred.idxs[i]), 
								sensor_num(pred.idxs[i]) );
					}
					printf("\r\n");
				}
				
				break;
			
			case CONVERT_SENSOR:
				strncpy ( tmpStr, req.name, 12 );

				// Get the corresponding index
				shReply.idx = model_nameToIdx(&rp.model, tmpStr);
				// Get the sensor id
				shReply.idx = idxTosIdx (shReply.idx, req.name);
				
				debug ("converted %s to %d\r\n", req.name, shReply.idx);
				// Reply to the shell
				Reply(senderTid, (char*)&shReply, sizeof(RPShellReply));
				break;

			case CONVERT_IDX:
				// Convert the node name into the node's index
				shReply.idx = model_nameToIdx(&rp.model, req.name);
				
				// Reply to the shell
				Reply(senderTid, (char*)&shReply, sizeof(RPShellReply));

				break;

			case RESERVE:
				// TODO: Reply with success?
				rp_reserve (&rp, &req);
				
				// Reply to the client train
				Reply(senderTid, (char*)&trReply, sizeof(RPReply));
				
				break;
			
			case PLANROUTE:
				// Determine the shortest path 
				rp_planRoute (&rp, &trReply, &req);
					
				// Reply to the client train
				Reply(senderTid, (char*)&trReply, sizeof(RPReply));
				break;

			case MIN_SENSOR_DIST:
				tmp = rp_minSensorDist (&rp, req.sensor1, req.sensor2);
				Reply( senderTid, (char*)&tmp, sizeof(int) );
				break;

			default:
				// This will never be called since we handle errors above.
				break;
		}
		
		// Clear the trReply for the next time we use it
		clearReply (&trReply);
	}

	Exit(); // This will never be called

}

int rp_init(RoutePlanner *rp) {
	char ch;
	int  shellTid;
	int  track;
	int	 err = NO_ERROR;

	// Get the model from the shell
	Receive(&shellTid, (char*)&ch, sizeof(ch));
	if ( ch != 'A' && ch != 'a' && ch != 'B' && ch != 'b' ) {
		err = INVALID_TRACK;
	}
	Reply  (shellTid,  (char*)&err, sizeof(int));
	
	debug ("routeplanner: Using track %c\r\n", ch);

	// Parse the model for this track
	track = ( ch == 'A' || ch == 'a' ) ? TRACK_A : TRACK_B;
	parse_model( track, &rp->model );
	
	// Initialize shortest paths using model
	floyd_warshall (rp, rp->model.num_nodes); 
	/*	
	int j, k;
	debug ("FWARSH: %d\r\n", Time(WhoIs(CLOCK_NAME))*MS_IN_TICK);
	for ( j = 0; j < rp.model.num_nodes; j ++ ) {
		for ( k = 0; k < rp.model.num_nodes; k ++ ) {
		 // print out stuff ..
		}
	}
	debug ("FWARSH: %d\r\n", Time(WhoIs(CLOCK_NAME))*MS_IN_TICK);
	*/

	// TODO: Initialize track reservation system (nothing is reserved)

	// Register with the Name Server
	RegisterAs ( ROUTEPLANNER_NAME );

	return shellTid;
}

int rp_errorCheckShRequest (RoutePlanner *rp, RPRequest *req) {
	int err;
	char tmpName[5];

	// Unfortunately, we need to copy this since model_nametoIdx changes it
	strncpy (tmpName, req->name, 5);

	// Check request type.
	if ( (req->type < 0) || (req->type > MIN_SENSOR_DIST) ) {
		return RP_INVALID_REQ_TYPE;
	}
	
	// Check the name of the node.
	if ( (req->name != 0) && 
		 (err = model_nameToIdx(&rp->model, tmpName)) < NO_ERROR ) {
		return err;
	}

	// Error check the node indicies.
	if ( (req->nodeIdx1 < 0) || (req->nodeIdx1 > rp->model.num_nodes) ||
		 (req->nodeIdx2 < 0) || (req->nodeIdx2 > rp->model.num_nodes) ) {
		return INVALID_NODE_IDX;
	}
	
	return NO_ERROR;
}

int rp_errorCheckTrRequest (RoutePlanner *rp, RPRequest *req) {
	// Check request type.
	if ( (req->type < 0) || (req->type > MIN_SENSOR_DIST) ) {
		return RP_INVALID_REQ_TYPE;
	}

	// Check the sensor
	if ( (req->sensor1 < 0) || (req->sensor1 > 80) ||
		 (req->sensor2 < 0) || (req->sensor2 > 80) ) {
		return INVALID_SENSOR_IDX;
	}

	// Check the train id.
	if ( req->trainId != 12 && req->trainId != 22 && req->trainId != 24 &&
		 req->trainId != 46 && req->trainId != 52 ) {
		return INVALID_TRAIN;
	}

	// Check the last hit sensor.
	if ( (req->lastSensor < 0) || (req->lastSensor > 80) ) {
		debug ("invalid sensor index: %d\r\n", req->lastSensor);
		return INVALID_SENSOR_IDX;
	}

	// Check the train's average speed.
	if ( (req->avgSpeed < 0) || (req->avgSpeed > 1000) ) {
		return INVALID_TRAIN_SPEED;
	}

	// Check destination index.
	if ( (req->destIdx < 0) || (req->destIdx > rp->model.num_nodes) ) {
		return INVALID_NODE_IDX;
	}

	return NO_ERROR;
}

inline void clearReply (RPReply *trReply) {
	trReply->err      = NO_ERROR;
	trReply->stopDist = 0;
}

// ----------------------------------------------------------------------------
// --------------------------- Route Finding ----------------------------------
// ----------------------------------------------------------------------------

int oppositeSensorId (int sensorIdx) {
//	debug ("opposite to %d is %d\r\n", sensorIdx, (sensorIdx ^1));
	return (sensorIdx ^ 1);
}

void rp_planRoute ( RoutePlanner *rp, RPReply *trReply, RPRequest *req ) {
	debug ("PLANNING A ROUTE\r\n");
	int  nextRv;
	int  totalDist;
	int  currentIdx = sIdxToIdx(req->lastSensor);
	Path p;

	// Distance from current sensor to destination node
	totalDist = rp->dists[currentIdx][req->destIdx];

	// Construct the path from current sensor to destination node
	makePath ( rp, &p, currentIdx, req->destIdx );
	rp_displayPath ( rp, &p );
	
		// Get the distance to the next reverse
	nextRv = rp_distToNextRv (rp, &p);

	// The stop distance is min {next reverse, total distance to travel}
	if ( nextRv < totalDist ) {
		trReply->stopDist = nextRv;
		debug ("Stop distance is a reverse\r\n");
	} else {
		trReply->stopDist = totalDist;
		debug ("Stop distance is the total distance.\r\n");
	}
	debug ("Stopping distance is: %d\r\n", trReply->stopDist);

	// Clear the next sensor predictions
	trReply->nextSensors.len = 0;

	// If the train needs to turn around right now,
	if (rp_turnAround (rp, &p, req->lastSensor) ) {
		debug ("Train needs to turn around first.\r\n");
		trReply->stopDist = -trReply->stopDist;
		
		// Predicting sensors we expect to hit.
		// Then, we'll predict the behind sensors.
		rp_predictSensors(rp, &trReply->nextSensors, oppositeSensorId(req->lastSensor));
	}	

	// Predict the next sensors the train could hit
 	rp_predictSensors (rp, &trReply->nextSensors, req->lastSensor);
	
	printf ("Predicted Sensors: ");
	for ( i = 0; i < trReply->nextSensors.len; i ++ ) {
		printf ("%c%d, ", sensor_bank(trReply->nextSensors.idxs[i]), 
				sensor_num(trReply->nextSensors.idxs[i]) );
	}
	printf("\r\n");

	// Get the next switches 
	rp_getNextSwitchSettings (rp, &p, (SwitchSetting*)trReply->switches);
	
	printf ("Switch settings: \r\n");
	for ( i = 0; i < 3; i ++ ) {
		printf("i=%d dist=%d id=%d dir=%d\r\n", i, 
				trReply->switches[i].dist, trReply->switches[i].id, 
				trReply->switches[i].dir);
	}


}

int rp_turnAround ( RoutePlanner *rp, Path *p, int sensorId ) {
	if ( p->len < 2 ) {
		return 0;
	}
	
	Node *sensor = &rp->model.nodes[sIdxToIdx(sensorId)];
	int   even   = (sensorId %2) == 0;

	if ( (sensor->se.behind.dest  == p->path[1] && even) ||
		 (sensor->se.ahead.dest == p->path[1] && !even) ) {
		return 1;
	}
	
	return 0;
}

int rp_distToNextRv (RoutePlanner *rp, Path *p) {
	int  i;
	int  *path = p->path;
	Node *itr;
	
	for ( i = 1; i < p->len - 1; i ++ ) {
		itr = &rp->model.nodes[path[i]];

		// If you pass a switch, you might need to reverse.
		if ( itr->type == NODE_SWITCH ) {
			//Node *prev = &rp->model.nodes[path[i-1]];
			//Node *next = &rp->model.nodes[path[i+1]];

			// If you need to follow BOTH "ahead" edges of a switch,
			// REVERSE.
			if ( ((itr->sw.ahead[0].dest == path[i+1]) ||
				  (itr->sw.ahead[1].dest == path[i+1])) &&
				 ((itr->sw.ahead[0].dest == path[i-1]) ||
				  (itr->sw.ahead[1].dest == path[i-1])) ) {
//				debug ("Next reverse is %s\r\n", rp->model.nodes[path[i]].name);
				return rp->dists[path[0]][path[i]] + EPSILON;
			}
		}
	}
	
	return INT_MAX;
}

int rp_distToNextSw (RoutePlanner *rp, Path *p) {
	int *path = p->path;
	Node *itr;

	// Start at i=1 to skip first node
	// Stop at len - 1 since we don't have to switch the last node if we
	// want to stop on it
	int i;
	for ( i = 1; i < p->len - 1; i ++ ) {
		itr = &rp->model.nodes[path[i]];
		
		if ( itr->type == NODE_SWITCH ) {
		//	Node *prev = &rp->model.nodes[path[i-1]];
			
			// If coming from either "ahead" edges, you don't need to 
			// change a switch direction.
			if ( !((itr->sw.ahead[0].dest == path[i-1]) || 
				 (itr->sw.ahead[1].dest == path[i-1])) ) {
//				debug ("Next switch is %s\r\n", rp->model.nodes[path[i]].name);
				return rp->dists[path[0]][path[i]] - EPSILON;
			}
		}
	}

	return INT_MAX;
}

void rp_getNextSwitchSettings (RoutePlanner *rp, Path *p, SwitchSetting *settings) {
	int *path = p->path;
	Node *itr;
	int n = 0;

	// Start at i=1 to skip first node
	// Stop at len - 1 since we don't have to switch the last node if we
	// want to stop on it
	int i;
	for ( i = 1; i < p->len - 1; i ++ ) {
		itr = &rp->model.nodes[path[i]];
		
		if ( itr->type == NODE_SWITCH ) {
			
			// If coming from either "ahead" edges, you don't need to 
			// change a switch direction.
			if ( !((itr->sw.ahead[0].dest == path[i-1]) || 
				 (itr->sw.ahead[1].dest == path[i-1])) ) {
//				debug ("Next switch is %s\r\n", rp->model.nodes[path[i]].name);
				
				settings[n].dist = rp->dists[path[0]][path[i]];
				settings[n].id   = itr->id;
				settings[n].dir  = getSwitchDir(itr, &rp->model.nodes[path[i+1]]);
				n ++;
			}

		// STOP WHEN YOU DON'T HIT A SWITCH
		} else {
			break;
		}
	}

	// Set the rest of the settings to invalid.
	for ( ; n < 3; n ++ ) {
		settings[n].dist = -1;
		settings[n].id   = -1;
		settings[n].dir  = -1;
	}
}

void rp_predictSensors (RoutePlanner *rp, SensorsPred *pred, int sensorId ) {
	int idx = sIdxToIdx ( sensorId );
	Node *n = &rp->model.nodes[idx];
	Edge *e = ((sensorId % 2) == 0) ? &n->se.ahead : &n->se.behind; // next edge
	
	// Get next node along the path
	n = &rp->model.nodes[e->dest]; 

	// Fill the "path" with sensor ids
	rp_predictSensorHelper ( rp, pred, n, idx );
}

void rp_predictSensorHelper ( RoutePlanner *rp, SensorsPred *pred,
							  Node *n, int prevIdx ) {
	Node *n1, *n2;
	
	switch ( n->type ) {
		case NODE_SWITCH:
			// Progress on an edge you didn't just come from
			
			// Get next nodes along the path
			if ( n->sw.ahead[0].dest == prevIdx ) {
				n1 = &rp->model.nodes[n->sw.behind.dest]; 
				
				// Recurse on the next node
				rp_predictSensorHelper ( rp, pred, n1, n->idx );
			
			} else if ( n->sw.ahead[1].dest == prevIdx ) {
				n1 = &rp->model.nodes[n->sw.behind.dest]; 
				
				// Recurse on the next node
				rp_predictSensorHelper ( rp, pred, n1, n->idx );
			} else { //behind
				n1 = &rp->model.nodes[n->sw.ahead[0].dest]; 
				n2 = &rp->model.nodes[n->sw.ahead[1].dest]; 
				
				// Recurse on these next nodes
				rp_predictSensorHelper ( rp, pred, n1, n->idx );
				rp_predictSensorHelper ( rp, pred, n2, n->idx );
			}
			break;
		
		case NODE_SENSOR:
			// Store the sensor id
			pred->idxs[pred->len] = (n->idx * 2);
			if ( n->se.ahead.dest == prevIdx ) {
				pred->idxs[pred->len] += 1;
			}

			// Advance length of the "path"
			pred->len ++;
			break;
	
		case NODE_STOP:
			break;
	}
}

int rp_minSensorDist ( RoutePlanner *rp, int sensor1, int sensor2 ) {
	int idx1 = sIdxToIdx (sensor1);
	int idx2 = sIdxToIdx (sensor2);

	return rp->dists[idx1][idx2];
}

//-----------------------------------------------------------------------------
//--------------------- Displaying To Monitor ---------------------------------
//-----------------------------------------------------------------------------

void rp_outputPath (RoutePlanner *rp, int i, int j) {
	if ( rp->paths[i][j] == -1 ) {
		if ( rp->model.nodes[i].type == NODE_SENSOR ) {
			printf ("%c%d> ", 
					sensor_bank(rp->model.nodes[i].id), 
					sensor_num( rp->model.nodes[i].id));
		} else {
			printf ("%s> ", rp->model.nodes[i].name);
		}
	}
	else {
		rp_outputPath (rp, i, rp->paths[i][j]);
		rp_outputPath (rp, rp->paths[i][j], j);
	}
}

void rp_displayFirstSw (RoutePlanner *rp, RPRequest *req) {
	Path p;
	int dist;
	int i = 0;
	
	makePath ( rp, &p, req->nodeIdx1, req->nodeIdx2 );
	dist = rp_distToNextSw(rp, &p);
	
	if ( dist == INT_MAX ) {
		printf ("No switch to change along path from %s to %s.\r\n", 
				rp->model.nodes[req->nodeIdx2].name, 
				rp->model.nodes[p.path[i]].name);
		return;
	}

	while ( (rp->dists[req->nodeIdx1][p.path[i]] != dist) && 
			(i < p.len) ) {
		i++;
	}
	printf("%s %s to %s is %s. Distance to it is %d.\r\n",
			"First switch to change from",
			rp->model.nodes[req->nodeIdx1].name, 
			rp->model.nodes[req->nodeIdx2].name, 
			rp->model.nodes[p.path[i]].name,
			dist);
}

void rp_displayFirstRv (RoutePlanner *rp, RPRequest *req) {
	int i = 0;
	int dist;
	Path p;

	makePath ( rp, &p, req->nodeIdx1, req->nodeIdx2 );
	dist = rp_distToNextRv(rp, &p);
	
	if ( dist == INT_MAX ) {
		printf ("Never reverse along path from %s to %s.\r\n", 
				rp->model.nodes[req->nodeIdx2].name, 
				rp->model.nodes[p.path[i]].name);
		return;
	}

	while ( (rp->dists[req->nodeIdx1][p.path[i]] != (dist + EPSILON)) && 
			(i < p.len) ) {
		i++;
	}
	
	printf("%s %s to %s is %s. Distance to it is %d.\r\n",
			"First reverse from",
			rp->model.nodes[req->nodeIdx1].name, 
			rp->model.nodes[req->nodeIdx2].name, 
			rp->model.nodes[p.path[i]].name,
			dist);
}

void rp_displayPath ( RoutePlanner *rp, Path *p ) {
	int n = p->len;
	int i;
	
	for ( i = 0; i < n; i ++ ) {
		printf ("%s> ", rp->model.nodes[p->path[i]].name);
	}

	printf("\r\n");
}
//-----------------------------------------------------------------------------
//--------------------------- Reservation Stuff -------------------------------
//-----------------------------------------------------------------------------

void rp_reserve (RoutePlanner *rp, RPRequest *req) {
	Reservation *rsv = &rp->reserves[mapTrainId(req->trainId)];

	// Cancel the old reservations
	cancelReserve(&rp->model, rsv);

	// Save the new reservation data
/*	//rsv->dist  = req->dist;
	rsv->start = req->nodeA;
	model_findNextNodes( &rp->model, req->nodeA, req->nodeB, 
						 rsv->next1, rsv->next2 ); 
*/
	// Make this new reservation
	int retDist = makeReserve(&rp->model, rsv);

}

void cancelReserve(TrackModel *model, Reservation *rsv) {
	// Have: start node & distance & 2 next nodes
	rsv->start->reserved = 0;
	rsv->next1->reserved = 0;
	rsv->next2->reserved = 0;
}

int makeReserve(TrackModel *model, Reservation *rsv) {

	// Reserve these nodes so that no other train can use them.
	rsv->start->reserved = 1;
	rsv->next1->reserved = 1;
	rsv->next2->reserved = 1;

	// TODO: fix this
	return 0;
}

// And the hardcoding begins!
int mapTrainId (int trainId) {
	switch (trainId) {
		case 12:
			return 0;
		case 22:
			return 1;
		case 24:
			return 2;
		case 46:
			return 3;
		case 52:
			return 4;
	}
	// ERROR
	return 5;
}

//-----------------------------------------------------------------------------
//------------------------------ Floyd Warshall -------------------------------
//-----------------------------------------------------------------------------
// O(n^3) + cost lookup time
//
// Floyd-Warshall algorithm from http://www.joshuarobinson.net/docs/floyd_warshall.html.
// 
//  Solves the all-pairs shortest path problem using Floyd-Warshall algorithm
//  Input:  n - number of nodes
//          model - To get the cost information.
//  Output: retDist - shortest path dists(the answer)
//          retPred - predicate matrix, useful in reconstructing shortest routes
void floyd_warshall ( RoutePlanner *rp, int n ) {
	int  i, j, k; // Loop counters
	
	// Algorithm initialization
	for ( i = 0; i < n; i++ ) {
		for ( j = 0; j < n; j++ ) {
			
			// INT_MAX if no link; 0 if i == j
			rp->dists[i][j] = cost(&rp->model, i, j); 
			
			// Init to garbage
			rp->paths[i][j] = -1;
		}
	}

  	// Main loop of the algorithm
	for ( k = 0; k < n; k++ ) {
		for ( i = 0; i < n; i++ ) {
			for ( j = 0; j < n; j++ ) {
				
				if (rp->dists[i][j] > (rp->dists[i][k] + rp->dists[k][j])) {
	  				
					rp->dists[i][j] = rp->dists[i][k] + rp->dists[k][j];
	  				rp->paths[i][j] = k; 
				}
      		}
    	}
  	}

	/*
	// Print out the results table of shortest rp->distss
  	for ( i = 0; i < 1; i++ ) {
    	for ( j = 0; j < n; j++ ) {
      		
			if ( rp->dists[i][j] != INT_MAX ) {
		//		debug("DIST: (%s,%s)=%d THROUGH ", model->nodes[i].name, 
		//									   model->nodes[j].name, 
			//								   rp->dists[i][j]);
			rp_outputPath(model, rp, i ,j);		
			printf ("%s\r\n", model->nodes[j].name);
			}
		}
    }
	*/
}  //end of floyd_warshall()

int cost (TrackModel *model, int idx1, int idx2) {
	//debug ("cost: i=%d j=%d model=%x\r\n", idx1, idx2, model);
	int itr = 0;
	int i, j;

	if ( idx1 == idx2 ) {
		return 0;
	}

	// Find the lower # of edges to reduce search time
	if ( model->nodes[idx1].type < model->nodes[idx2].type ) {
		i = idx1;
		j = idx2;
	} else {
		i = idx2;
		j = idx1;
	}
	
	// TODO: Is this correct?
	// If either are reserved, let's say there is no link to them.
	if ( (model->nodes[i].reserved == 1) || 
		 (model->nodes[j].reserved == 1) ) {
		return INT_MAX;
	}

	// Check each edge in O(3) time
	while ( itr < (int) model->nodes[i].type ) {
		// If the edge reaches the destination,
		if ( model->nodes[i].edges[itr].dest == j ) {
			// Return the distance between them
			return model->nodes[i].edges[itr].distance; 
		}

		// Advance the iterator
		itr += 1;
	}
	return INT_MAX;
}

void makePath (RoutePlanner *rp, Path *p, int i, int j) {
	
	// Initialize the length of this path
	p->len = 0;
	
	// Make the path from i -> (j-1)
	makePathHelper (rp, p, i, j);
	
	// Stick on the last node.
	if ( i != j ) {
		p->path[p->len] = j;
		p->len += 1;
	}

/*
	int  k;
	int *path = p->path;
//	debug ( "path len %d\r\n", p->len );
	// path[0] == current node. Start checking after it.
	for ( k = 0; k < p->len; k ++ ) {
		//debug ("k=%d name=%s type=%d path=%d\r\n", k, rp->model.nodes[path[k]].name,
		//										rp->model.nodes[path[k]].type, path[k]);
		debug("%s> ", rp->model.nodes[path[k]].name);
	}
	debug ("\r\n");
	*/

}

// UGLY & FULL 'O HACKS
// but it works ...
void makePathHelper (RoutePlanner *rp, Path *p, int i, int j) {

	if ( rp->paths[i][j] == -1 ) {
		p->path[p->len] = i;
		p->len += 1;

	} else {
		makePathHelper (rp, p, i, rp->paths[i][j]);
		makePathHelper (rp, p, rp->paths[i][j], j);
	}
}
