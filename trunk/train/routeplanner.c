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
#include "servers.h"
#include "train.h"

#define INT_MAX			0xFFFF
#define REVERSE_COST 	100

// Private Stuff
// ----------------------------------------------------------------------------

typedef struct {
	int len;		// Number of reserved sensors
	int idxs[8];	// Indicies of the reserved sensor nodes
} Reservation;

typedef struct {
	int 		dists[MAX_NUM_NODES][MAX_NUM_NODES];
	int			paths[MAX_NUM_NODES][MAX_NUM_NODES];
	int 		pred[MAX_NUM_NODES][MAX_NUM_NODES];
	int			succ[MAX_NUM_NODES][MAX_NUM_NODES];

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
void rp_predictSensorHelper(RoutePlanner *rp, SensorsPred *p, Node *n, int prevIdx, int startIdx);
int  rp_minSensorDist 	(RoutePlanner *rp, int sensor1, int sensor2 );

// Reservation Functions
void rp_reserve			(RoutePlanner *rp, SensorsPred *sensors, int trainId);
void rsv_cancel			(Reservation *rsv, TrackModel *model);
void rsv_make			(Reservation *rsv, TrackModel *model, SensorsPred *sensors);
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
int rev 				(RoutePlanner *rp, int i, int j, int k);
void dijkstra		 	(TrackModel *model, int source, int dest);

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
	Path			p;

	// Initialize the Route Planner
	shellTid = rp_init (&rp);

	FOREVER {
		// Receive from a client train
		Receive ( &senderTid, (char*)&req, sizeof(RPRequest) );
		//printf("routeplanner: Rcvd sender=%d from=%d type=%d idx1=%d idx2=%d\r\n",
		//	  senderTid, req.trainId, req.type, req.nodeIdx1, req.nodeIdx2);

		// Error check the shell's request.
		if ( senderTid == shellTid ) {
			if( ( err = rp_errorCheckShRequest( &rp, &req )) < NO_ERROR ) {
				req.type = ERROR;	// Make sure we don't do any work below.
				Reply(senderTid, (char*)&err, sizeof(int));
			} 
		// Error check a train's request.
		} else {
			if( (err = rp_errorCheckTrRequest(&rp, &req)) < NO_ERROR ) {
				req.type = ERROR;	// Make sure we don't do any work below.
				Reply ( senderTid, (char*)&err, sizeof(int) );
			}
		}

		switch ( req.type ) {
			case ERROR:
				// Do nothing since we handled the error already!
				break;

			case DISPLAYROUTE:
				// Reply to the shell
				Reply(senderTid, (char*)&shReply, sizeof(RPShellReply));

				debug ("determining the shortest path from %d (%d) to %d\r\n", 
						sIdxToIdx(req.lastSensor), req.lastSensor, req.destIdx);
				rp_planRoute (&rp, &trReply, &req);
				
				makePath ( &rp, &p, sIdxToIdx(req.lastSensor), req.destIdx );
				rp_displayPath ( &rp, &p );
				
				// Display the total distance
				printf ("\r\nDistance travelled = %d\r\n", 
						rp.dists[sIdxToIdx(req.lastSensor)][req.destIdx]);
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
						printf ("%c%d (dist=%d), ", sensor_bank(pred.idxs[i]), 
								sensor_num(pred.idxs[i]), pred.dists[i] );
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
				
				debug ("converted %s to %d\r\n", req.name, shReply.idx);
				// Reply to the shell
				Reply(senderTid, (char*)&shReply, sizeof(RPShellReply));

				break;

			case RESERVE:
				//TODO: rp_reserve (&rp, &req);
				
				// Reply to the client train
				Reply(senderTid, (char*)&trReply, sizeof(RPReply));
				
				break;
			
			case PLANROUTE:
				// Determine the shortest path 
				// Make a route
//				debug ("determining the shortest path from %d (%d) to %d\r\n", 
//						sIdxToIdx(req.lastSensor), req.lastSensor, req.destIdx);
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
	int i;

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

	// Initialize track reservation system (nothing is reserved)
	for ( i = 0; i < MAX_NUM_TRAINS; i ++ ) {
		rp->reserves[i].len = 0;
	}

	// Register with the Name Server
	RegisterAs ( ROUTEPLANNER_NAME );

	return shellTid;
}

int rp_errorCheckShRequest (RoutePlanner *rp, RPRequest *req) {
	int  err;
	char tmpName[5];

	// Check request type.
	
	switch (req->type) {
		case CONVERT_SENSOR:
		case CONVERT_IDX:
			// Unfortunately, we need to copy this since model_nametoIdx changes it
			strncpy (tmpName, req->name, 5);
			
			// Check the name of the node.
			if ( (err = model_nameToIdx(&rp->model, tmpName) ) < NO_ERROR ) {
				return err;
			}
			break;
		
		case DISPLAYROUTE:
		case DISPLAYFSTSW:
		case DISPLAYFSTRV:
			// Error check the node indicies.
			if ( (req->nodeIdx1 < 0) || (req->nodeIdx1 > rp->model.num_nodes) ||
				 (req->nodeIdx2 < 0) || (req->nodeIdx2 > rp->model.num_nodes) ) {
				return INVALID_NODE_IDX;
			}
			break;
		case DISPLAYPREDICT:
			// Error check the node indicies.
			if ( (req->nodeIdx1 < 0) || (req->nodeIdx1 > rp->model.num_nodes) ) {
				return INVALID_NODE_IDX;
			}
			break;
		default: 
			return RP_INVALID_REQ_TYPE;
			break;
	}
	
	return NO_ERROR;
}

int rp_errorCheckTrRequest (RoutePlanner *rp, RPRequest *req) {
	
	switch (req->type) {

		case RESERVE:
			// Check the train id.
			if ( req->trainId != 12 && req->trainId != 22 && req->trainId != 24 &&
				req->trainId != 46 && req->trainId != 52 ) {
				return INVALID_TRAIN;
			}
		//	TODO:Check something else
			break;
				
		case PLANROUTE:
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
			break;
	
		case MIN_SENSOR_DIST:
			// Check the sensor
			if ( (req->sensor1 < 0) || (req->sensor1 > 80) ||
				(req->sensor2 < 0) || (req->sensor2 > 80) ) {
				return INVALID_SENSOR_IDX;
			}
			break;
		default:
			return RP_INVALID_REQ_TYPE;
			break;
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
	int  nextRv;
	int  totalDist;
	int  currentIdx = sIdxToIdx(req->lastSensor);
	Path p;
	debug ("GOING TO NODE %s(%d)\r\n", rp->model.nodes[req->destIdx].name, req->destIdx);


	// Distance from current sensor to destination node
	totalDist = rp->dists[currentIdx][req->destIdx];
	//debug ("totalDist = %d\r\n", totalDist);
	
	// Construct the path from current sensor to destination node
	makePath ( rp, &p, currentIdx, req->destIdx );
//	rp_displayPath ( rp, &p );
	
	// Get the distance to the next reverse
	nextRv = rp_distToNextRv (rp, &p);
//	debug ("TotalDist=%d Reverse Distance=%d\r\n", totalDist, nextRv);

	// The stop distance is min {next reverse, total distance to travel}
	if ( nextRv < totalDist ) {
		trReply->stopDist = nextRv;
		trReply->stopAction = STOP_AND_REVERSE;
//		debug ("Stop distance is a reverse. (%d)\r\n", nextRv);
	} else {
		trReply->stopDist   = totalDist;
		trReply->stopAction = JUST_STOP;
//		debug ("Stop distance is the total distance. (%d)\r\n", totalDist);
	}
//	debug ("Stopping distance is: %d\r\n", trReply->stopDist);

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
/*	
	debug ("Predicted Sensors: ");
	for ( i = 0; i < trReply->nextSensors.len; i ++ ) {
		printf ("%c%d, ", sensor_bank(trReply->nextSensors.idxs[i]), 
				sensor_num(trReply->nextSensors.idxs[i]) );
	}
	printf("\r\n");
*/
	// Get the next switches 
	rp_getNextSwitchSettings (rp, &p, (SwitchSetting*)trReply->switches);
	/*
	int i;
	for ( i = 0; i < NUM_SETTINGS; i ++ ) {
		if ( trReply->switches[i].dist == -1 ) {
			break;
		}
		printf ("Switch settings: \r\n");

		printf("i=%d dist=%d id=%d dir=%d\r\n", i, 
				trReply->switches[i].dist, trReply->switches[i].id, 
				trReply->switches[i].dir);
	}
*/
	// Reserve the next sensors
	rp_reserve( rp, &trReply->nextSensors, req->trainId );
}

int rp_turnAround ( RoutePlanner *rp, Path *p, int sensorId ) {
	if ( p->len < 2 ) {
		return 0;
	}
	
//	debug ("TURNAROUND:\r\n");

	Node *sensor = &rp->model.nodes[sIdxToIdx(sensorId)];
	int   even   = (sensorId %2) == 0;
//	debug ("sIdx=%d idx=%d node=%s even?%d \r\n", sensorId, sIdxToIdx(sensorId), sensor->name, even);

//	debug ("nextNode=%d Behind=%d ahead=%d\r\n", p->path[1], sensor->se.behind.dest, sensor->se.ahead.dest);
	if ( (sensor->se.behind.dest  == p->path[1] && even) ||
		 (sensor->se.ahead.dest == p->path[1] && !even) ) {
		
		return 1;
	}
	
//	debug (" DONE TURN AROUND\r\n");
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
				return rp->dists[path[0]][path[i]];
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
				return rp->dists[path[0]][path[i]];
			}
		}
	}

	return INT_MAX;
}

void rp_getNextSwitchSettings (RoutePlanner *rp, Path *p, SwitchSetting *settings) {
	int *path = p->path;
	Node *itr = &rp->model.nodes[path[0]];
	Node *prevItr;
	int n = 0;
	int maxDist = 500;

	debug ("Getting the switch settings for this path: \r\n");

	int i;
	for ( i = 0; i < p->len; i ++ ) {
		debug ("(%d)%s>", path[i], rp->model.nodes[path[i]].name);
	}
	debug ("\r\n");

	// Start at i=1 to skip first node
	// Stop at len - 1 since we don't have to switch the last node if we
	// want to stop on it
	for ( i = 1; i < p->len - 1; i ++ ) {
		prevItr = itr;
		itr     = &rp->model.nodes[path[i]];
		
		if ( itr->type == NODE_SWITCH ) {
//			debug ("Switch: %s (%d), \r\n", itr->name, path[i]);
			// If coming from either "ahead" edges, you don't need to 
			// change a switch direction.
			if ( (itr->sw.ahead[0].dest == path[i+1]) || (itr->sw.ahead[1].dest == path[i+1]) ) {
//				debug ("behind=%d ahead[0]=%d ahead[1]=%d next along path=(%d) %s\r\n", 
//					itr->sw.behind.dest, 
//					itr->sw.ahead[0].dest, 
//					itr->sw.ahead[1].dest,
//					path[i+1], 
//					rp->model.nodes[path[i+1]].name);
//			if ( !((() && (itr->sw.ahead[0].dest == path[i-1])) || ((itr->sw.ahead[0].dest != path[i+1]) && (itr->sw.ahead[1].dest == path[i-1]))) ) {
//				debug ("Next switch is %s\r\n", rp->model.nodes[path[i]].name);
				
//				debug ("adding this setting: \r\n");
				settings[n].dist = rp->dists[path[0]][path[i]];
				settings[n].id   = itr->id;
				settings[n].dir  = getSwitchDir(itr, &rp->model.nodes[path[i+1]]);
//				debug ("n:%d dist:%d id:%d dir:%d\r\n",
//						n, settings[n].dist, settings[n].id, settings[n].dir);
				n ++;

				assert ( n < NUM_SETTINGS );
			}
		}

		// Subtract from the distance as we travel further
		maxDist -= rp->dists[prevItr->idx][itr->idx];

		if ( (maxDist <= 0) && (n == 5) ) {
			break;
		}
	}

	// Set the rest of the settings to invalid.
	for ( ; n < NUM_SETTINGS; n ++ ) {
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
	rp_predictSensorHelper ( rp, pred, n, idx, idx );
}

void rp_predictSensorHelper ( RoutePlanner *rp, SensorsPred *pred,
							  Node *n, int prevIdx, int startIdx ) {
	Node *n1, *n2;
	
	switch ( n->type ) {
		case NODE_SWITCH:
			// Progress on an edge you didn't just come from
			
			// Get next nodes along the path
			if ( n->sw.ahead[0].dest == prevIdx ) {
				n1 = &rp->model.nodes[n->sw.behind.dest]; 
				
				// Recurse on the next node
				rp_predictSensorHelper ( rp, pred, n1, n->idx, startIdx );
			
			} else if ( n->sw.ahead[1].dest == prevIdx ) {
				n1 = &rp->model.nodes[n->sw.behind.dest]; 
				
				// Recurse on the next node
				rp_predictSensorHelper ( rp, pred, n1, n->idx, startIdx );
			} else { //behind
				n1 = &rp->model.nodes[n->sw.ahead[0].dest]; 
				n2 = &rp->model.nodes[n->sw.ahead[1].dest]; 
				
				// Recurse on these next nodes
				rp_predictSensorHelper ( rp, pred, n1, n->idx, startIdx );
				rp_predictSensorHelper ( rp, pred, n2, n->idx, startIdx );
			}
			break;
		
		case NODE_SENSOR:
			// Store the sensor id
			pred->idxs[pred->len] = (n->idx * 2);
			if ( n->se.ahead.dest == prevIdx ) {
				pred->idxs[pred->len] += 1;
			}
			
			// Store the distance from start node to here.
			pred->dists[pred->len] = rp->dists[startIdx][n->idx];
				
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

	while ( (rp->dists[req->nodeIdx1][p.path[i]] != dist) && 
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
	
	debug ("Path: ");
	for ( i = 0; i < n; i ++ ) {
		printf ("%s> ", rp->model.nodes[p->path[i]].name);
//		debug ("%s(%d)>", p->path[i], rp->model.nodes[p->path[i]].name);
	}

	printf("\r\n");
}
//-----------------------------------------------------------------------------
//--------------------------- Reservation Stuff -------------------------------
//-----------------------------------------------------------------------------

// TODO: Rewrite all of this
void rp_reserve (RoutePlanner *rp, SensorsPred *sensors, int trainId) {
	Reservation *rsv = &rp->reserves[mapTrainId(trainId)];

	// Cancel the old reservations
	rsv_cancel( rsv, &rp->model );

	// Make this new reservation
	rsv_make( rsv, &rp->model, sensors );

	// Recompute the distance tables
	// TODO: dijkstra ?
	floyd_warshall( rp, rp->model.num_nodes );
}

void rsv_cancel( Reservation *rsv, TrackModel *model ) {
	int i;
	int index;

	for ( i = 0; i < rsv->len; i ++ ) {
		index = rsv->idxs[i];
	//	printf ("Cancelling %s(%d)\r\n", model->nodes[index].name, index);

		// Make sure the sensor node is actually reserved
		assert ( model->nodes[index].reserved == 1 );
		
		// Cancel the reservation
		model->nodes[index].reserved = 0;
	
		rsv->idxs[i] = -1;
	}

	rsv->len = 0;
}

void rsv_make( Reservation *rsv, TrackModel *model, SensorsPred *sensors ) {
	int i;
	int index;
	
	for ( i = 0; i < sensors->len; i ++ ) {
		// Convert the sensor index to node index
		index = sIdxToIdx( sensors->idxs[i] );
		//printf ("Reserving %s(%d)\r\n", model->nodes[index].name, index);

		// Make sure this node is not already reserved
		assert ( model->nodes[index].reserved == 0 );
		
		// Reserve the sensor node
		model->nodes[index].reserved = 1;

		// Store the node index in the reservation
		rsv->idxs[i] = index;
	}

	// Save the number of nodes in this reservation
	rsv->len = sensors->len;
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
	int  costMod = 0;
	
	// Algorithm initialization
	for ( i = 0; i < n; i++ ) {
		for ( j = 0; j < n; j++ ) {
			
			// INT_MAX if no link; 0 if i == j
			rp->dists[i][j] = cost(&rp->model, i, j); 
			
			// Init to garbage
			rp->paths[i][j] = -1;

			rp->pred[i][j] = i;
			rp->succ[i][j] = j;
		}
	}

  	// Main loop of the algorithm
	for ( k = 0; k < n; k++ ) {
		for ( i = 0; i < n; i++ ) {
			for ( j = 0; j < n; j++ ) {
				
				costMod = (rev( rp, rp->pred[i][k], rp->succ[k][j], k ) == 1) ? REVERSE_COST : 0;

				if (rp->dists[i][j] > (rp->dists[i][k] + rp->dists[k][j] + costMod)) {
	  				
					rp->dists[i][j] = rp->dists[i][k] + rp->dists[k][j] + costMod;
	  				rp->paths[i][j] = k; 

					rp->pred[i][j] = rp->pred[k][j];
					rp->succ[i][j] = rp->succ[i][k];
				}
      		}
    	}
  	}
	/*
	Path p;
	for ( i = 0; i < 1; i ++ ) {
		for ( j = 0; j < 10; j ++ ) {

			makePath (rp, &p, i, j);
			debug ("PATH from %s(%d) to %s(%d):\r\n", rp->model.nodes[i].name, i, rp->model.nodes[j].name, j );

			int s = i;
			int pr = j;
			for ( k = 0; k < p.len; k ++ ) {
				s = rp->succ[s][j];
				pr = rp->pred[i][pr];
				
				debug ("s=%s(%d) pr=%s(%d) p=%s(%d) k=%d\r\n", rp->model.nodes[s].name, s, rp->model.nodes[pr].name, pr, rp->model.nodes[p.path[k]].name, p.path[k], k);
				
				//assert (next == p.path[k]);
			}
			printf ("(%d, %d) PASSED.\r\n", i, j);
		}
	}
	*/
}  //end of floyd_warshall()


int rev (RoutePlanner *rp, int i, int j, int k) {
	Node *mid = &rp->model.nodes[k];

	if ( mid->type == NODE_SWITCH ) {
		if ( ((mid->sw.ahead[0].dest == i) || (mid->sw.ahead[1].dest == i)) &&
		     ((mid->sw.ahead[0].dest == j) || (mid->sw.ahead[1].dest == j )) ) {
			return 1;
		}	
	}
	return 0;
}
int cost (TrackModel *model, int idx1, int idx2) {
//	debug ("cost: i=%d j=%d model=%x\r\n", idx1, idx2, model);
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
	
	// TODO: Reservation Stuff
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
	debug ( "path len %d\r\n", p->len );
	// path[0] == current node. Start checking after it.
	for ( k = 0; k < p->len; k ++ ) {
		//debug ("k=%d name=%s type=%d path=%d\r\n", k, rp->model.nodes[path[k]].name,
		//										rp->model.nodes[path[k]].type, path[k]);
		printf("%s(%d)> ", rp->model.nodes[path[k]].name, path[k]);
	}
	debug ("\r\n");*/

}

// UGLY & FULL 'O HACKS
// but it works ...
void makePathHelper (RoutePlanner *rp, Path *p, int i, int j) {
	assert ( p->len < MAX_NUM_NODES );
	assert ( &p->path[p->len] < &p->path[MAX_NUM_NODES] );


	if ( rp->paths[i][j] == -1 ) {
		p->path[p->len] = i;
		p->len += 1;

	} else {
		makePathHelper (rp, p, i, rp->paths[i][j]);
		makePathHelper (rp, p, rp->paths[i][j], j);
	}
}

void dijkstra ( TrackModel *model, int source, int dest ) {
	int n = model->num_nodes;
	
	int dists[n];
	int prev [n];
	int visited [n];
	int min;
	int linkCost;
	int i, j;

	for ( i = 0; i < n; i ++ ) {
		dists[i] 	= INT_MAX;
		prev[i]  	= INT_MAX; 
		visited[i] 	= 0;
	}

	dists[source] = 0;

	for ( i = 1; i < n; i ++ ) {
		min = INT_MAX;
		for ( j = 0; j < n; j ++ ) {
			if ( !visited[j] && 
				 (min == INT_MAX || dists[j] < dists[min]) ) {
				min = j;
			}
		}

		visited[min] = 1;

		// Until the destination node,
		for ( j = 1; j <= dest; j ++ ) {
			linkCost = cost( model, min, j );
			
			if ( linkCost + dists[min] < dists[j] ) {
				dists[j] = dists [min] + linkCost;
				prev [j] = min;
			}
		}
	}

	printf ("Dist from %s to %d is=%d.\r\n", source, dest, dists[dest]);

	for ( i = 0; i < n; i ++ ) {
		printf ("%d> ", prev[i]);

		if ( prev[i] == INT_MAX ) break;
	}
}
