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

#define REVERSE_COST 	0
#define INT_MAX			0xFFFF

// Private Stuff
// ----------------------------------------------------------------------------

// A prediction for all of the NODES a train could hit
typedef struct {
	int len;
	int idxs[20];
} NodePred;



inline SwitchDir getSwitchDir(Node *sw, Node *next);

int rp_init      		(RoutePlanner *rp);
int rp_errorCheckShRequest (RoutePlanner *rp, RPRequest *req);
int rp_errorCheckTrRequest (RoutePlanner *rp, RPRequest *req);
inline void clearReply  (RPReply *trReply);

// Route Finding Functions
void rp_planRoute 		(RoutePlanner *rp, RPReply *trReply, RPRequest *req);
int  rp_turnAround 		(RoutePlanner *rp, Path *p, int sensorId);
int  rp_distToNextRv   	(RoutePlanner *rp, Path *p);
void rp_getNextSwitchSettings (RoutePlanner *rp, Path *p, SwitchSetting *settings);

// Node and Sensor Prediction Functions
void rp_predict 		(RoutePlanner *rp, SensorsPred *p, NodePred *nodePred,
						 int sensorId);

int  rp_minSensorDist 	(RoutePlanner *rp, int sensor1, int sensor2 );

// Display to Monitor Functions
void rp_displayFirstRv 	(RoutePlanner *rp, RPRequest *req);
void rp_displayPath	  	(RoutePlanner *rp, Path *p );

// Shortest Path Algorithms
int  cost 				(TrackModel *model, int idx1, int idx2,
						 int rsvDist, int trainId);
int  rev				(RoutePlanner *rp, Node *prev, Node *mid, Node *next);

// ahead[0] = straight
// ahead[1] = curved
inline SwitchDir getSwitchDir (Node *sw, Node *next) {
	Edge *e = sw->sw.ahead[0];

	return (node_neighbour( sw, e ) == next) ? SWITCH_STRAIGHT : SWITCH_CURVED;
}

// ----------------------------------------------------------------------------
void rp_run() {
	debug ("rp_run\r\n");
	RoutePlanner	rp;
	int 			senderTid;
	RPRequest		req;
	RPReply			trReply;
	RPShellReply    shReply;
	int 			shellTid;
	SensorsPred		sensPred;
	NodePred		nodePred;
	int 			i;
	int				err;
	int				tmp;
	char 			tmpStr[12];
	Path			p;

	// Initialize the Route Planner
	shellTid = rp_init (&rp);

	FOREVER {
		
		// Clear the trReply for the next time we use it
		clearReply (&trReply);
		
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
				
				//rp_displayFirstSw (&rp, &req);

				break;

			case DISPLAYFSTRV:
				// Reply to the shell
				Reply(senderTid, (char*)&shReply, sizeof(RPShellReply));
				
				rp_displayFirstRv (&rp, &req);
				break;
			
			case DISPLAYPREDICT:
				// Reply to the shell
				Reply(senderTid, (char*)&shReply, sizeof(RPShellReply));
				
				sensPred.len = 0;
				nodePred.len = 0;

				rp_predict( &rp, &sensPred, &nodePred, 
							idxTosIdx(req.nodeIdx1, req.name) );
				
				if ( req.nodeIdx1 > 39 ) {
					printf ("\033[36m%s is not a valid sensor.\033[37m\r\n", req.name);
				} else if ( sensPred.len == 0 ) {
					printf ("There are no sensors.\r\n");
				} else {
					printf ("Sensors: ");
					for ( i = 0; i < sensPred.len; i ++ ) {
						printf ("%c%d (dist=%d), ", sensor_bank(sensPred.idxs[i]), 
								sensor_num(sensPred.idxs[i]), sensPred.dists[i] );
					}
					printf("\r\nNodes: ");
					for ( i = 0; i < nodePred.len; i ++ ) {
						printf ("%s, ", rp.model.nodes[nodePred.idxs[i]].name );
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
				
				printf ("converted %s to %d\r\n", req.name, shReply.idx);
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
	}

	Exit(); // This will never be called

}

int rp_init(RoutePlanner *rp) {
	char ch;
	int  shellTid;
	int  track;
	int	 err = NO_ERROR;
	int  resTid;
	TrackModel *modelPtr = &rp->model;

	// Get the model from the shell
	Receive(&shellTid, (char*)&ch, sizeof(ch));
	if ( ch != 'A' && ch != 'a' && ch != 'B' && ch != 'b' ) {
		err = INVALID_TRACK;
	}
	Reply  (shellTid,  (char*)&err, sizeof(int));
	//debug ("routeplanner: Using track %c\r\n", ch);

	// Parse the model for this track
	track = ( ch == 'A' || ch == 'a' ) ? TRACK_A : TRACK_B;
	parse_model( track, &rp->model );
	
	// Receive the reservation server's tid from the shell
	Receive(&shellTid, &resTid, sizeof(int));
	
	// HACK: Give the reservation server a pointer to our model.
	Send( resTid, &modelPtr, sizeof(int), 0, 0 );
	
	// Initialize shortest paths using model
	floyd_warshall (rp, &rp->model, 0); 

	// Register with the Name Server
	RegisterAs ( ROUTEPLANNER_NAME );
	
	// Reply to the shell after registering
	Reply  (shellTid, 0, 0 );

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
		case RESERVE:
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
//			if ( req->trainId != 12 && req->trainId != 22 && req->trainId != 24 &&
//				req->trainId != 46 && req->trainId != 52 ) {
//				return INVALID_TRAIN;
//			}
		//	TODO:Check something else
			break;
				
		case PLANROUTE:
			// Check the train id.
//			if ( req->trainId != 12 && req->trainId != 22 && req->trainId != 24 &&
//				req->trainId != 46 && req->trainId != 52 ) {
//				return INVALID_TRAIN;
//			}

			// Check the last hit sensor.
			if ( (req->lastSensor < 0) || (req->lastSensor > 80) ) {
				debug ("invalid sensor index: %d\r\n", req->lastSensor);
				return INVALID_SENSOR_IDX;
			}
/*
			// Check the train's average speed.
			if ( (req->avgSpeed < 0) || (req->avgSpeed > 1000) ) {
				return INVALID_TRAIN_SPEED;
			}
*/
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
	Path p;					// Path to travel along to get to destination
	int  nextRv;			// Distance to the next reverse
	int  totalDist;			// Distance if reservations and blockages are considered
	int  actualDist;		// Actual distance if we don't consider reservations
	int  currentIdx = sIdxToIdx(req->lastSensor);
	NodePred   nodePred;
	
	debug ("GOING TO NODE %s(%d) from %s\r\n", 
			rp->model.nodes[req->destIdx].name, req->destIdx, rp->model.nodes[currentIdx].name);

	// TODO: Every time?
	//floyd_warshall( rp, rp->model.num_nodes, req->trainId );
	
	// Distance from current sensor to destination node
	actualDist = rp->dists[currentIdx][req->destIdx];
	totalDist  = rp->rsvDists[currentIdx][req->destIdx];
	debug ("totalDist = %d\r\n", totalDist);
	
	if ( totalDist >= INT_MAX ) {
		printf ("There is no path from %s to dest!\r\n", 
				rp->model.nodes[currentIdx].name);
		trReply->err      = NO_PATH;
		trReply->stopDist = 0;
		trReply->stopAction = JUST_STOP;
		return;
	}

	// Construct the path from current sensor to destination node
	makePath ( rp, &p, currentIdx, req->destIdx );
//	rp_displayPath ( rp, &p );
	
	// Get the distance to the next reverse
	nextRv = rp_distToNextRv (rp, &p);
//	debug ("TotalDist=%d Reverse Distance=%d\r\n", totalDist, nextRv);

	// The stop distance is min {next reverse, total distance to travel}
	if ( nextRv < totalDist ) {
		trReply->stopDist   = nextRv;
		trReply->stopAction = STOP_AND_REVERSE;
//		debug ("Stop distance is a reverse. (%d)\r\n", nextRv);
	} else {
		trReply->stopDist   = totalDist;
		trReply->stopAction = JUST_STOP;
//		debug ("Stop distance is the total distance. (%d)\r\n", totalDist);
	}

	// Clear the next sensor predictions
	trReply->nextSensors.len = 0;
	nodePred.len 			 = 0;

	// If the train needs to turn around right now,
	if (rp_turnAround (rp, &p, req->lastSensor) ) {
		debug ("Train needs to turn around first.\r\n");
		trReply->stopDist = -trReply->stopDist;
		
		// Predicting sensors we expect to hit.
		// Then, we'll predict the behind sensors.
		rp_predict( rp, &trReply->nextSensors, &nodePred,
						   oppositeSensorId(req->lastSensor) );
	}	

	// Predict the next sensors the train could hit
 	rp_predict( rp, &trReply->nextSensors, &nodePred, req->lastSensor );
	debug ("predicted %d sensors\r\n", &trReply->nextSensors.len);
	// Get the next switches 
	rp_getNextSwitchSettings( rp, &p, (SwitchSetting*)trReply->switches );

/*
  	int i;
	debug ("Predicted Sensors: ");
	for ( i = 0; i < trReply->nextSensors.len; i ++ ) {
		printf ("%c%d, ", sensor_bank(trReply->nextSensors.idxs[i]), 
				sensor_num(trReply->nextSensors.idxs[i]) );
	}
	printf("\r\n");
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
}

int rp_turnAround ( RoutePlanner *rp, Path *p, int sensorId ) {
	if ( p->len < 2 ) {
		return 0;
	}
	
//	debug ("TURNAROUND:\r\n");

	Node *sensor = &rp->model.nodes[sIdxToIdx(sensorId)];
	Node *next = &rp->model.nodes[p->path[1]];
	// next edge
	Edge *e = ((sensorId % 2) == 0) ? sensor->se.behind : sensor->se.ahead; 
	
	return( node_neighbour( sensor, e ) == next );
}

int rp_distToNextRv (RoutePlanner *rp, Path *p) {
	int  i;
	int  *path = p->path;
	
	if( p->len < 2 ) return INT_MAX;

	Node *prev;
	Node *curr = &rp->model.nodes[path[0]];
	Node *next = &rp->model.nodes[path[1]];
	for ( i = 1; i < p->len - 1; i ++ ) {
		prev = curr;
		curr = next;
		next = &rp->model.nodes[path[i+1]];
		// If you pass a switch, you might need to reverse.
		if ( curr->type == NODE_SWITCH ) {
			if( rev( rp, prev, curr, next ) ) {
//				debug ("Next reverse is %s\r\n", rp->model.nodes[path[i]].name);
				return rp->dists[path[0]][path[i]];
			}
		}
	}
	
	return INT_MAX;
}

void rp_getNextSwitchSettings (RoutePlanner *rp, Path *p, SwitchSetting *settings) {
	int *path = p->path;
	Node *prev;
	Node *curr = &rp->model.nodes[path[0]];
	Node *next = &rp->model.nodes[path[1]];
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
		prev = curr;
		curr = next;
		next = &rp->model.nodes[path[i+1]];
		if ( curr->type == NODE_SWITCH ) {
			Node *behind = node_neighbour( curr, curr->sw.behind );
			if ( behind == prev ) {
				settings[n].dist = rp->dists[path[0]][path[i]];
				settings[n].id   = curr->id;
				settings[n].dir  = getSwitchDir(curr, next);
				n ++;
			
			} else { // curr.sw.behind = next
				settings[n].dist = rp->dists[path[0]][path[i]];
				settings[n].id   = curr->id;
				settings[n].dir  = getSwitchDir(curr, prev);
				n ++;
			}
			if( rev( rp, prev, curr, next ) ) {
				if( prev == behind ) {
					eprintf( "rp: revesing on a switch backwards\r\n" );
				// Switch one switch ahead to prevent multitrack drifting
				} else if( behind->type == NODE_SWITCH ) {

					settings[n].dist = rp->dists[path[0]][behind->idx];
					settings[n].id   = behind->id;
					settings[n].dir  = getSwitchDir(behind, curr);
					n ++;
				}
			}


			assert ( n < NUM_SETTINGS );

		}

		// Subtract from the distance as we travel further
		maxDist -= rp->dists[prev->idx][curr->idx];

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

// ----------------------------------------------------------------------------
// ---------------------- Node & Sensor Predictions  --------------------------
// ----------------------------------------------------------------------------
void rp_predictHelper( RoutePlanner *rp, SensorsPred *pred, NodePred *nodePred,
							  Node *n, Node *prev, int startIdx ) {
	Node *n1, *n2;
	
	// Save this node.
	nodePred->idxs[nodePred->len] = n->idx;
	nodePred->len += 1;
	assert( nodePred->len <= array_size( nodePred->idxs ) );

	switch ( n->type ) {
		case NODE_SWITCH:
			// Progress on an edge you didn't just come from
			
			// Get next nodes along the path
			if ( (node_neighbour( n, n->sw.ahead[0] ) == prev) ||
				 (node_neighbour( n, n->sw.ahead[1] ) == prev) ) {
				n1 = node_neighbour( n, n->sw.behind ); 
				
				// Recurse on the next node
				rp_predictHelper ( rp, pred, nodePred, n1, n, startIdx );
			
			} else { //behind
				n1 = node_neighbour( n, n->sw.ahead[0] ); 
				n2 = node_neighbour( n, n->sw.ahead[1] ); 
				
				// Recurse on these next nodes
				rp_predictHelper ( rp, pred, nodePred, n1, n, startIdx );
				rp_predictHelper ( rp, pred, nodePred, n2, n, startIdx );
			}
			break;
		
		case NODE_SENSOR:
			// Store the sensor id
			pred->idxs[pred->len] = (n->idx * 2);
			// These are the even numbered (odd indexed) sensors
			if ( node_neighbour( n, n->se.ahead ) == prev ) {
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

void rp_predict( RoutePlanner *rp, SensorsPred *pred, NodePred *nodePred, int sensorId ) {
	int idx = sIdxToIdx ( sensorId );
	// Get the start node
	Node *start = &rp->model.nodes[idx];
 	// Get the next edge
	Edge *e = ((sensorId % 2) == 0) ? start->se.ahead : start->se.behind;
	
	// Get next node along the path
	Node *n = node_neighbour( start, e ); 

	// Fill the "path" with sensor ids
	rp_predictHelper ( rp, pred, nodePred, n, start, idx );
}

int rp_minSensorDist ( RoutePlanner *rp, int sensor1, int sensor2 ) {
	int idx1 = sIdxToIdx (sensor1);
	int idx2 = sIdxToIdx (sensor2);

	return rp->dists[idx1][idx2];
}

//-----------------------------------------------------------------------------
//--------------------- Displaying To Monitor ---------------------------------
//-----------------------------------------------------------------------------


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
void floyd_warshall ( RoutePlanner *rp, TrackModel *model, int trainId ) {
	int  n = model->num_nodes;
	int  i, j, k; // Loop counters
	int  distMod = 0;
	
	// Algorithm initialization
	for ( i = 0; i < n; i++ ) {
		for ( j = 0; j < n; j++ ) {
			
			// INT_MAX if no link; 0 if i == j
			rp->dists[i][j]    = cost(model, i, j, 0, trainId); // no reservations
			rp->rsvDists[i][j] = cost(model, i, j, 1, trainId); // use reservations
			
			// Init to garbage
			//rp->paths[i][j] = -1;

			rp->pred[i][j] = i;
			rp->succ[i][j] = j;
		}
	}

  	// Main loop of the algorithm
	for ( k = 0; k < n; k++ ) {
		for ( i = 0; i < n; i++ ) {
			for ( j = 0; j < n; j++ ) {
				

				//if ( i == j ) break;

				distMod = 0;//rev( rp, rp->pred[i][k], rp->succ[k][j], k ) ? REVERSE_COST : 0;
			/*	if( distMod > 0 ) {
					printf("reversing at %s-%s-%s\r\n",
						rp->model.nodes[rp->pred[i][k]].name, 
						rp->model.nodes[k].name,
						rp->model.nodes[rp->succ[k][j]].name );
					bwgetc( COM2 );
				}*/
				// Consider the reservations while computing the distances
				if (rp->rsvDists[i][j] > (rp->rsvDists[i][k] + rp->rsvDists[k][j] + distMod) ) {
					rp->rsvDists[i][j] = rp->rsvDists[i][k] + rp->rsvDists[k][j] + distMod;

	  				// Store how we got here
				//	rp->paths[i][j]    = k; 
					rp->pred[i][j] = rp->pred[k][j];
					rp->succ[i][j] = rp->succ[i][k];
				}
				// Store the distances as if there are not reservations
				if (rp->dists[i][j] > (rp->dists[i][k] + rp->dists[k][j] + distMod)) {
					rp->dists[i][j] = rp->dists[i][k] + rp->dists[k][j] + distMod;
				}
      		}
    	}
  	}
	/*
	Path p;
	for ( i = 0; i < 1; i ++ ) {
		for ( j = 0; j < 10; j ++ ) {

			makePath (rp, &p, i, j);
			debug( "PATH from %s(%d) to %s(%d): (dist=%d)(rsvD=%d)\r\n", 
					rp->model.nodes[i].name, i, rp->model.nodes[j].name, j,
					rp->dists[i][j], rp->rsvDists[i][j]);

			int s = i;
			for ( k = 0; ; k ++ ) {
				s = rp->succ[s][j];
				
				debug ("%s(%d)> ", rp->model.nodes[s].name, s);
				
				if ( s == j ) break;
				//assert (next == p.path[k]);
			}
			debug ("\r\n");
			for ( k = 0; k < p.len; k ++ ) {
				debug ("%s(%d)> ", rp->model.nodes[p.path[k]].name, p.path[k]);
			}
			debug ("\r\n");
		}
	}
	*/
	
}  //end of floyd_warshall()

int rev( RoutePlanner *rp, Node *prev, Node *mid, Node *next ) {

	if ( mid->type == NODE_SWITCH ) {
		Node *straight 	= node_neighbour( mid, mid->sw.ahead[0] );
		Node *curved 	= node_neighbour( mid, mid->sw.ahead[1] );

		if(		((straight == prev) || (curved == prev))
			&&	((straight == next) || (curved == next)) ) {
			return 1;
		}	
	}
	return 0;
}
int cost (TrackModel *model, int idx1, int idx2, int rsvDist, int trainId) {
//	debug ("cost: i=%d j=%d model=%x\r\n", idx1, idx2, model);
	int itr;
	Node *node;
	Node *next;

	if ( idx1 == idx2 ) {
		return 0;
	}

	// Find the lower # of edges to reduce search time
	if ( model->nodes[idx1].type < model->nodes[idx2].type ) {
		node = &model->nodes[idx1];
		next = &model->nodes[idx2];
	} else {
		node = &model->nodes[idx2];
		next = &model->nodes[idx1];
	}
	
	// TODO: Reservation Stuff
	// If either are reserved, let's say there is no link to them.
	if ( (rsvDist == 1) &&
	     ((node->reserved == 1 && node->reserver != trainId) || 
		  (next->reserved == 1 && node->reserver != trainId)) ){
		return INT_MAX;
	}

	// Check each edge in O(3) time
	for ( itr = 0; itr < (int) node->type; itr ++ ) {
		// If the edge reaches the destination,
		if ( node_neighbour( node, node->edges[itr] ) == next ) {
			// Return the distance between them
			return node->edges[itr]->distance; 
		}
	}
	return INT_MAX;
}

void makePath (RoutePlanner *rp, Path *p, int i, int j) {
//	debug ("makePath: ");
	// Recover the path from the FW matrix
	int k, s = i;
	for( k = 0; s != j; k++ ) {
//		debug ("k=%d, s=%d \r\n", k, s);
		assert( s >= 0 && s < rp->model.num_nodes );
		p->path[k] = s;
		s = rp->succ[s][j];

	}
	assert( s == j );
	p->path[k] = j;
	p->len = k + 1;
	assert( p->len <= array_size( p->path ) );

//	debug ("Path: ");
//	for ( k = 0; k < p->len; k ++ ) {
//		debug ("%s(%d)> ", rp->model.nodes[p->path[k]].name, p->path[k]);
//	}
//	debug ("\r\n");
}

void dijkstra ( TrackModel *model, int source, int dest, int *prev ) {
	int n = model->num_nodes;
	
	int dists[n];
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
			if ( !visited[j] && (min == INT_MAX || dists[j] < dists[min]) ) {
				min = j;
			}
		}

		visited[min] = 1;

		// Until the destination node,
		for ( j = 1; j <= dest; j ++ ) {
			linkCost = cost( model, min, j, 0, 0 ); //TODO: fix train id
			
			if ( linkCost + dists[min] < dists[j] ) {
				dists[j] = dists[min] + linkCost;
				prev [j] = min;
			}
		}
	}

	printf ("Dist from %d to %d is=%d.\r\n", source, dest, dists[dest]);

	for ( i = 0; i < n; i ++ ) {
		printf ("%d> ", prev[i]);

		if ( prev[i] == INT_MAX ) break;
	}
}
