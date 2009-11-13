/**
 * CS 452: User Task
 * becmacdo
 * dgoc
 *
 * FUNCTION NAME SPECIFICATION
 * Please use the following name convention.
 *
 * If a function can take in a Node OR index number, create 2 functions for ease:
 *  fooN (..., Node *node, ...) 
 *  fooI (..., int idx, ...)
 *
 * If a function can take in 2 Nodes OR a Path, create 2 functions for ease:
 * 	barN (..., Node *a, Node *b, ...)
 * 	barP (..., Path *p, ...)
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

void rp_init();
void rp_planRoute (RoutePlanner *rp, RPReply *reply, RPRequest *req);

inline int rp_minDistN 		 (RoutePlanner *rp, Node *a, Node *b);
inline int rp_minDistI 		 (RoutePlanner *rp, int idx1, int idx2 );
inline int rp_neighbourDistN (RoutePlanner *rp, Node *a, Node *b);
inline int rp_neighbourDistI (RoutePlanner *rp, int idx1, int idx2);

//int rp_getNextCheckinN  (RoutePlanner *rp, Node *a, Node *b);
int rp_getNextCheckinP 	(RoutePlanner *rp, Path *path, RPReply *rpl); 
int rp_distToNextSw   	(RoutePlanner *rp, Path *p, RPReply *rpl);
SwitchDir rp_getSwitchDir(RoutePlanner *rp, Node *sw, Node *next);
int rp_distToNextRv   	(RoutePlanner *rp, Path *p, RPReply *rpl);
void rp_predictSensors  (RoutePlanner *rp, Path *p, int sensorId );
void rp_predictSensorHelper (RoutePlanner *rp, Path *p, Node *n, int prevIdx);

inline void clearReply (RPReply *reply);

// Reservation Functions
void rp_reserve   (RoutePlanner *rp, RPRequest *req);
void cancelReserve(TrackModel *model, Reservation *rsv);
int  makeReserve  (TrackModel *model, Reservation *rsv);
int  mapTrainId   (int trainId);

// Display to Monitor Functions
void outputPath 	   (RoutePlanner *rp, int i, int j);
void rp_displayFirstSw (RoutePlanner *rp, RPRequest *req);
void rp_displayFirstRv (RoutePlanner *rp, RPRequest *req);
void rp_displayPath (RoutePlanner *rp, Path *p );

// Shortest Path Algorithms
void floyd_warshall (RoutePlanner *rp, int n);
int  cost			(TrackModel *model, int idx1, int idx2);
void makePath 		(RoutePlanner *rp, Path *p, int i, int j);
void makePathHelper (RoutePlanner *rp, Path *p, int i, int j);
// ----------------------------------------------------------------------------

void rp_run() {
	debug ("rp_run\r\n");
	RoutePlanner	rp;
	int 			senderTid;
	RPRequest		req;
	RPReply			reply;
	Path			p;
	int 			i;

	rp_init ();

	// HACK
	parse_model( TRACK_A, &rp.model );

	// TODO: Get the model from the first task
	// Send ();

	// Initialize shortest paths using model
	floyd_warshall (&rp, rp.model.num_nodes); 
/*	
	int j, k;
	debug ("FWARSH: %d\r\n", Time(WhoIs(CLOCK_NAME))*MS_IN_TICK);
	for ( j = 0; j < rp.model.num_nodes; j ++ ) {
		for ( k = 0; k < rp.model.num_nodes; k ++ ) {
			//debug ("IDX CHECK: (%d, %d) & (%d, %d)\r\n", j, rp.model.nodes[j].idx, k, rp.model.nodes[k].idx);
			rp_getNextCheckin ( &rp,
								&rp.model.nodes[j],
								&rp.model.nodes[k] );
			Getc(WhoIs(SERIALIO2_NAME));
		}
	}
	debug ("FWARSH: %d\r\n", Time(WhoIs(CLOCK_NAME))*MS_IN_TICK);
*/
	// Initialize track reservation system (nothing is reserved)
	

	FOREVER {
		// Receive from a client train
		Receive ( &senderTid, (char*)&req, sizeof(RPRequest) );
//		debug ("routeplanner: received a message sender=%d from=%d type=%d idx1=%d idx2=%d PLAN=%d\r\n", 
//				senderTid, req.trainId, req.type, req.idx1, req.idx2, PLANROUTE);

		switch ( req.type ) {
			case DISPLAYROUTE:
				// Reply to the shell
				Reply(senderTid, (char*)&reply, sizeof(RPReply));

				// If we are within the bounds of nodes:
				if ( (req.idx1 < rp.model.num_nodes) && 
					 (req.idx2 < rp.model.num_nodes) ) {
					// Display the path
					outputPath ( &rp, req.idx1, req.idx2 );
					// Display the total distance
					printf ("%s\r\nDistance travelled = %d\r\n", 
							rp.model.nodes[req.idx2].name,
							rp_minDistN( &rp, 
										 &rp.model.nodes[req.idx1], 
										 &rp.model.nodes[req.idx2]) );
				}
				break;

			case DISPLAYFSTSW:
				// Reply to the shell
				Reply(senderTid, (char*)&reply, sizeof(RPReply));
				
				rp_displayFirstSw (&rp, &req);

				break;

			case DISPLAYFSTRV:
				// Reply to the shell
				Reply(senderTid, (char*)&reply, sizeof(RPReply));
				
				rp_displayFirstRv (&rp, &req);
				break;
			
			case DISPLAYPREDICT:
				// Reply to the shell
				Reply(senderTid, (char*)&reply, sizeof(RPReply));
				
				rp_predictSensors(&rp, &p, req.idx1);
				if ( p.len == 0 ) {
					printf ("There are no sensors.\r\n");
				} else {
					printf ("Sensors: ");
					for ( i = 0; i < p.len; i ++ ) {
						printf ("%c%d, ", sensor_bank(p.path[i]), sensor_num(p.path[i]) );
					}
					printf("\r\n");
				}

				break;
			case RESERVE:
				// TODO: Reply with success?
				rp_reserve (&rp, &req);
				
				// Reply to the client train
				Reply(senderTid, (char*)&reply, sizeof(RPReply));
				
				break;
			
			case PLANROUTE:
				debug ("routeplanner: planning a route for tr %d\r\n", req.trainId);
				// Determine the shortest path 
				rp_planRoute (&rp, &reply, &req);
					
				// Reply to the client train
				Reply(senderTid, (char*)&reply, sizeof(RPReply));
				break;

			//TODO: Make this reply with jsut an int?
			case MINDIST:
				reply.minDist = rp_minDistI (&rp, req.idx1, req.idx2);

				Reply(senderTid, (char*)&reply, sizeof(RPReply));
				break;

			//TODO: Make this reply with jsut an int?
			case NEIGHBOURDIST:
				// Returns -1 if they are not neighbours. Distance otherwise
				reply.minDist = rp_neighbourDistI (&rp, req.idx1, req.idx2);
				
				Reply(senderTid, (char*)&reply, sizeof(RPReply));
				break;

		}
		
		// Clear the reply for the next time we use it
		clearReply (&reply);
	}

	Exit(); // This will never be called

}

void rp_init() {
	RegisterAs ( ROUTEPLANNER_NAME );
}

void rp_planRoute ( RoutePlanner *rp, RPReply *reply, RPRequest *req ) {
	Path p;

	// Distance from current location to destination
	reply->totalDist = rp_minDistI ( rp, req->idx1, req->dest );

	// Construct the path from i -> j
	makePath ( rp, &p, req->idx1, req->dest );

	rp_getNextCheckinP (rp, &p, reply);
	
	 
	/*
	int q = 0;
	debug("pathlen = %d\r\n", reply->pathLen);
	for ( q = 0; q < reply->pathLen; q ++ ) {
		debug("%d> ", reply->path[q]);
	}
	*/
}

inline int rp_minDistN (RoutePlanner *rp, Node *a, Node *b) {
	return rp_minDistI(rp, a->idx, b->idx);
}

inline int rp_minDistI (RoutePlanner *rp, int idx1, int idx2 ) {
	return rp->dists[idx1][idx2];
}

inline int rp_neighbourDistN (RoutePlanner *rp, Node *a, Node *b) {
	return rp_neighbourDistI(rp, a->idx, b->idx);	
}

inline int rp_neighbourDistI (RoutePlanner *rp, int idx1, int idx2 ) {
	int c = cost (&rp->model, idx1, idx2);

	return ( c == INT_MAX ) ? -1 : c;
}
/*
int rp_getNextCheckinN (RoutePlanner *rp, Node *a, Node *b) {
	debug ("Making a path from %s(%d) to %s(%d)\r\n", 
			a->name, a->idx, b->name, b->idx);
	
	Path path;
	makePath ( rp, &path, a->idx, b->idx );
	
	return rp_getNextCheckinP(rp, &path);

}
*/
int rp_getNextCheckinP ( RoutePlanner *rp, Path *p, RPReply *reply ) {
	int nextSw = rp_distToNextSw(rp, p, reply);
	int nextRv = rp_distToNextRv(rp, p, reply);

	if ( nextSw == nextRv && nextSw == INT_MAX ) {
		debug ("%s to %s: Neither.\r\n", 
				rp->model.nodes[p->path[0]].name, 
				rp->model.nodes[p->path[p->len-1]].name);
		
		reply->reverse   = 0;
		reply->switchId  = -1;
		reply->switchDir = 0;

		reply->checkinDist = reply->totalDist;
	}
	else if ( nextSw < nextRv ) {
		debug ("%s to %s: First Checkin=SWITCH dist=%d\r\n", 
				rp->model.nodes[p->path[0]].name, 
				rp->model.nodes[p->path[p->len-1]].name, nextSw);
		
		reply->checkinDist = nextSw;
		
	} else {
		debug ("%s to %s: First Checkin=REVERSE dist=%d\r\n", 
				rp->model.nodes[p->path[0]].name, 
				rp->model.nodes[p->path[p->len]].name, nextRv);
		
		// Tell the train to reverse at next checkin
		reply->reverse   = 1;
		reply->switchId  = -1;
		reply->switchDir = 0;
		reply->checkinDist = nextRv;
	}

	// TODO: fix this error(?) return.
	// There is nothing to do on the path but DRIVE.
	if ( nextSw == nextRv && nextSw == INT_MAX ) {
		return INT_MAX;
	}
	return (nextSw < nextRv) ? nextSw : nextRv;
}

int rp_distToNextSw (RoutePlanner *rp, Path *p, RPReply *reply) {
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
				
				// Give the train the switch info
				reply->switchId = itr->id;
				reply->switchDir = rp_getSwitchDir (rp, itr, 
												&rp->model.nodes[path[i+1]]);

				return rp->dists[path[0]][path[i]] - EPSILON;
			}
		}
	}

	return INT_MAX;
}

// ahead[0] = straight
// ahead[1] = curved
SwitchDir rp_getSwitchDir (RoutePlanner *rp, Node *sw, Node *next) {
	Edge *e = &sw->sw.ahead[0];

	return ( e->dest == next->idx ) ? SWITCH_STRAIGHT : SWITCH_CURVED;
}

int rp_distToNextRv (RoutePlanner *rp, Path *p, RPReply *reply) {
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

// 0 = A1
// 1 = A2
// 2 = A3
// 3 = A4
void rp_predictSensors (RoutePlanner *rp, Path *p, int sensorId ) {
	int   idx =  rp->model.sensor_nodes[sensorId];
	Node *n   = &rp->model.nodes[idx];
	Edge *e   = ((sensorId % 2) == 0) ? &n->se.ahead : &n->se.behind; // next edge
	
	// Get next node along the path
	n = &rp->model.nodes[e->dest]; 

	// Clear the path
	p->len = 0;

	// Fill the "path" with sensor ids
	rp_predictSensorHelper ( rp, p, n, idx );
}

void rp_predictSensorHelper (RoutePlanner *rp, Path *p, Node *n, int prevIdx) {
	Node *n1, *n2;
	
	switch ( n->type ) {
		case NODE_SWITCH:
			// Progress on an edge you didn't just come from
			
			// Get next nodes along the path
			if ( n->sw.ahead[0].dest == prevIdx ) {
				n1 = &rp->model.nodes[n->sw.behind.dest]; 
				
				// Recurse on the next node
				rp_predictSensorHelper ( rp, p, n1, n->idx );
			
			} else if ( n->sw.ahead[1].dest == prevIdx ) {
				n1 = &rp->model.nodes[n->sw.behind.dest]; 
				
				// Recurse on the next node
				rp_predictSensorHelper ( rp, p, n1, n->idx );
			} else { //behind
				n1 = &rp->model.nodes[n->sw.ahead[0].dest]; 
				n2 = &rp->model.nodes[n->sw.ahead[1].dest]; 
				
				// Recurse on these next nodes
				rp_predictSensorHelper ( rp, p, n1, n->idx );
				rp_predictSensorHelper ( rp, p, n2, n->idx );
			}
			break;
		
		case NODE_SENSOR:
			// Store the sensor id
			p->path[p->len] = (n->idx * 2);
			if ( n->se.ahead.dest == prevIdx ) {
				p->path[p->len] += 1;
			}

			// Advance length of the "path"
			p->len ++;
			break;
	
		case NODE_STOP:
			break;
	}
}

inline void clearReply (RPReply *reply) {
	//TODO

}

//-----------------------------------------------------------------------------
//--------------------- Displaying To Monitor ---------------------------------
//-----------------------------------------------------------------------------

void outputPath (RoutePlanner *rp, int i, int j) {
	if ( rp->paths[i][j] == -1 ) {
		printf ("%s> ", rp->model.nodes[i].name);
	}
	else {
		outputPath (rp, i, rp->paths[i][j]);
		outputPath (rp, rp->paths[i][j], j);
	}
}

void rp_displayFirstSw (RoutePlanner *rp, RPRequest *req) {
	Path p;
	int dist;
	int i = 0;
	RPReply reply;
	
	makePath ( rp, &p, req->idx1, req->idx2 );
	dist = rp_distToNextSw(rp, &p, &reply);
	
	if ( dist == INT_MAX ) {
		printf ("No switch to change along path from %s to %s.\r\n", 
				rp->model.nodes[req->idx2].name, 
				rp->model.nodes[p.path[i]].name);
		return;
	}

	while ( (rp->dists[req->idx1][p.path[i]] != (dist - EPSILON)) && 
			(i < p.len) ) {
		i++;
	}
	printf("%s %s to %s is %s. Distance to it is %d.\r\n",
			"First switch to change from",
			rp->model.nodes[req->idx1].name, 
			rp->model.nodes[req->idx2].name, 
			rp->model.nodes[p.path[i]].name,
			dist);
}

void rp_displayFirstRv (RoutePlanner *rp, RPRequest *req) {
	int i = 0;
	int dist;
	Path p;
	RPReply reply;

	makePath ( rp, &p, req->idx1, req->idx2 );
	dist = rp_distToNextRv(rp, &p, &reply);
	
	if ( dist == INT_MAX ) {
		printf ("Never reverse along path from %s to %s.\r\n", 
				rp->model.nodes[req->idx2].name, 
				rp->model.nodes[p.path[i]].name);
		return;
	}

	while ( (rp->dists[req->idx1][p.path[i]] != (dist - EPSILON)) && 
			(i < p.len) ) {
		i++;
	}
	
	printf("%s %s to %s is %s. Distance to it is %d.\r\n",
			"First reverse from",
			rp->model.nodes[req->idx1].name, 
			rp->model.nodes[req->idx2].name, 
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
			outputPath(model, rp, i ,j);		
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
