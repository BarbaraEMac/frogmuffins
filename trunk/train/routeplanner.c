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
#define EPSILON		0

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
inline void clearReply (RPReply *reply);
inline int rp_minDistance (RoutePlanner *rp, Node *a, Node *b);
inline int rp_neighbourDist (RoutePlanner *rp, Node *a, Node *b);

int rp_getNextCheckin (RoutePlanner *rp, Node *a, Node *b);
int rp_distToNextSw (RoutePlanner *rp, Path *p);
int rp_distToNextRv (RoutePlanner *rp, Path *p);

void model_cancelReserve(TrackModel *model, Reservation *rsv);
int model_makeReserve(TrackModel *model, Reservation *rsv);
void model_findNextNodes( TrackModel *model, Node *curr, Node *prev,
						  Node *next1, Node *next2 );
int mapTrainId (int trainId);


// Shortest Path Algorithms
void floyd_warshall (RoutePlanner *rp, int n);
int  cost			(TrackModel *model, int idx1, int idx2);
void outputPath 	(RoutePlanner *rp, int i, int j);
void makePath 		(RoutePlanner *rp, Path *p, int i, int j);
void makePathHelper (RoutePlanner *rp, Path *p, int i, int j);
// ----------------------------------------------------------------------------

void rp_planRoute ( RoutePlanner *rp, RPReply *reply, RPRequest *req ) {

	// Distance from current location to destination
	reply->dist = rp_minDistance ( rp, req->nodeA, req->nodeB );

	// Construct the path from i -> j
	// TODO: Observe blockages / reservations
	
	 
	 /*
	int q = 0;
	debug("pathlen = %d\r\n", reply->pathLen);
	for ( q = 0; q < reply->pathLen; q ++ ) {
		debug("%d> ", reply->path[q]);
	}
	*/

}

void rp_reserve (RoutePlanner *rp, RPRequest *req) {
	Reservation *rsv = &rp->reserves[mapTrainId(req->trainId)];

	// Cancel the old reservations
	model_cancelReserve(&rp->model, rsv);

	// Save the new reservation data
	//rsv->dist  = req->dist;
	rsv->start = req->nodeA;
	model_findNextNodes( &rp->model, req->nodeA, req->nodeB, 
						 rsv->next1, rsv->next2 ); 

	// Make this new reservation
	int retDist = model_makeReserve(&rp->model, rsv);

}

void model_cancelReserve(TrackModel *model, Reservation *rsv) {
	// Have: start node & distance & 2 next nodes
	rsv->start->reserved = 0;
	rsv->next1->reserved = 0;
	rsv->next2->reserved = 0;
}

int model_makeReserve(TrackModel *model, Reservation *rsv) {

	// Reserve these nodes so that no other train can use them.
	rsv->start->reserved = 1;
	rsv->next1->reserved = 1;
	rsv->next2->reserved = 1;

	// TODO: fix this
	return 0;
}

void model_findNextNodes( TrackModel *model, Node *curr, Node *prev,
						  Node *next1, Node *next2 ) {
	Node *n1 = 0;	// Tmp neighbour nodes
	Node *n2 = 0;
	Node *n3 = 0;
	
	switch ( curr->type ) {
		case NODE_SWITCH:
			n1 = &model->nodes[curr->sw.ahead[0].dest];
			n2 = &model->nodes[curr->sw.ahead[1].dest];
			n3 = &model->nodes[curr->sw.behind.dest];
			
			//TODO: DEPENDS ON THE ROUTE
			if ( (n1 == prev) || (n2 == prev) ) {
				next1 = n3;
				next2 = 0;
			} else { 
				next1 = n1;
				next2 = n2;
			}
			
			break;
		
		case NODE_SENSOR:
			n1 = &model->nodes[curr->se.ahead.dest];
			n2 = &model->nodes[curr->se.behind.dest];
			
			next1 = ( n1 == prev ) ? n2 : n1;
			next2 = 0;

			break;
		
		case NODE_STOP:
			n1 = &model->nodes[curr->st.ahead.dest];

			// You are heading to a stop
			next1 = (n1 == prev) ? 0 : n1;
			next2 = 0;
			
			break;
	}
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

// tell the train to switch a flip

void rp_run() {
	debug ("rp_run\r\n");
	RoutePlanner	rp;
	int 			senderTid;
	RPRequest		req;
	RPReply			reply;

	rp_init ();

	// HACK
	parse_model( TRACK_A, &rp.model );

	// TODO: Get the model from the first task
	// Send ();

	// Initialize shortest paths using model
	floyd_warshall (&rp, rp.model.num_nodes); 
	
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

	// Initialize track reservation system (nothing is reserved)
	
	FOREVER {
		// Receive from a client train
		Receive ( &senderTid, (char*)&req, sizeof(RPRequest) );
		
		switch ( req.type ) {
			case RESERVE:

				break;
			
			case PLANROUTE:
				// Determine the shortest path 
				rp_planRoute (&rp, &reply, &req);
					
				// Reply to the client train
				Reply(senderTid, (char*)&reply, sizeof(RPReply));
				break;

			// Make this reply with jsut an int?
			case MINDIST:
				reply.dist = rp_minDistance (&rp, req.nodeA, req.nodeB);

				Reply(senderTid, (char*)&reply, sizeof(RPReply));
				break;

			// Make this reply with jsut an int?
			case NEIGHBOURDIST:
				// Returns -1 if they are not neighbours.Distance otherwise
				reply.dist = rp_neighbourDist (&rp, req.nodeA, req.nodeB);
				
				Reply(senderTid, (char*)&reply, sizeof(RPReply));
				break;

		}
		
		// Clear the reply for the next time we use it
		clearReply (&reply);
	}

	Exit(); // This will never be called

}

inline void clearReply (RPReply *reply) {
	//TODO

}

inline int rp_minDistance (RoutePlanner *rp, Node *a, Node *b) {
	return rp->dists[a->idx][b->idx];
}

inline int rp_neighbourDist (RoutePlanner *rp, Node *a, Node *b) {
	int c = cost (&rp->model, a->id, b->id);

	return ( c == INT_MAX ) ? -1 : c;
}

void rp_init() {
	RegisterAs ( ROUTEPLANNER_NAME );
}

int rp_getNextCheckin (RoutePlanner *rp, Node *a, Node *b) {
	Path path;
	debug ("Making a path from %s(%d) to %s(%d)\r\n", a->name, a->idx, b->name, b->idx);
	makePath ( rp, &path, a->idx, b->idx );
	
	int nextSw = rp_distToNextSw(rp, &path);
//	debug ("Path from %d to next sw is %d\r\n", path.path[0], nextSw);
	int nextRv = rp_distToNextRv(rp, &path);

	if ( nextSw == nextRv && nextSw == INT_MAX ) {
		debug ("%s to %s: Neither.\r\n", a->name, b->name);
	}
	else if ( nextSw < nextRv ) {
		debug ("%s to %s: First Checkin=SWITch dist=%d\r\n", a->name, b->name, nextSw);
	} else {
		debug ("%s to %s: First Checkin=REVERSE dist=%d\r\n", a->name, b->name, nextRv);
	}

	return (nextSw < nextRv) ? nextSw : nextRv;
}

int rp_distToNextSw (RoutePlanner *rp, Path *p) {
	int *path = p->path;
	Node *itr;

	// Start at i=1 to skip first node
	int i;
	for ( i = 1; i < p->len; i ++ ) {
		itr  = &rp->model.nodes[path[i]];
		
		if ( itr->type == NODE_SWITCH ) {
		//	Node *prev = &rp->model.nodes[path[i-1]];
			
			// If coming from either "ahead" edges, you don't need to 
			// change a switch direction.
			if ( !((itr->sw.ahead[0].dest == path[i-1]) || 
				 (itr->sw.ahead[1].dest == path[i-1])) ) {
				debug ("Next switch is %s\r\n", rp->model.nodes[path[i]].name);
				return rp->dists[path[0]][path[i]] - EPSILON;
			}
		}
	}

	return INT_MAX;
}

int rp_distToNextRv (RoutePlanner *rp, Path *p) {
	int *path = p->path;
	Node *itr;
	int i;
	
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
				
				debug ("Next reverse is %s\r\n", rp->model.nodes[path[i]].name);
				return rp->dists[path[0]][path[i]] + EPSILON;
			}
		}
	}
	
	return INT_MAX;
}

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
		// If the edge reaches the nodeBination,
		if ( model->nodes[i].edges[itr].dest == j ) {
			// Return the distance between them
			return model->nodes[i].edges[itr].distance; 
		}

		// Advance the iterator
		itr += 1;
	}
	return INT_MAX;
}

void outputPath (RoutePlanner *rp, int i, int j) {
	if ( rp->paths[i][j] == -1 ) {
		printf ("%s> ", rp->model.nodes[i].name);
	}
	else {
		outputPath (rp, i, rp->paths[i][j]);
		outputPath (rp, rp->paths[i][j], j);
	}
}

void makePath (RoutePlanner *rp, Path *p, int i, int j) {
	
	// Initialize the length of this path
	p->len = 0;
	
	outputPath(rp, i, j);
	debug("\r\n");

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
