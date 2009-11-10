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


// Private Stuff
// ----------------------------------------------------------------------------

typedef struct {
	// what will this look like? How do we implement it?
} Reservation;

typedef struct {
	int 		dists[MAX_NUM_NODES][MAX_NUM_NODES];
	int			paths[MAX_NUM_NODES][MAX_NUM_NODES];


	
} RoutePlanner;


void routeplanner_init();

// Shortest Path Algorithms
void fwarsh ( TrackModel *model, RoutePlanner *rp, int n );
int cost ( TrackModel *model, int idx1, int idx2 );
void outputPath (TrackModel *model, RoutePlanner *rp, int i, int j );
// ----------------------------------------------------------------------------

void routeplanner_run() {
	debug ("routeplanner_run\r\n");
	TrackModel  	model;
	RoutePlanner	rp;
	int 			senderTid;
	RPRequest		msg;

	// HACK
	parse_model( TRACK_B, &model );

	// TODO: Get the model from the first task
	// Send ();

	// Initialize shortest paths using model
	fwarsh (&model, &rp, model.num_nodes); 

	// Initialize track reservation system (nothing is reserved)
	
	FOREVER {
		Receive ( &senderTid, (char*)&msg, sizeof(RPRequest) );
		// Receive from client - current location, destination, speed?, current time?
	// Determine shortest path (observing blockages / reservations)
	// Return the speed and direction? Next landmark? (does train even know the track topology?)



	}

	Exit(); // This will never be called

}

void routeplanner_init() {
	



}

// O(n^3) + cost lookup time
//
// Floyd-Warshall algorithm from http://www.joshuarobinson.net/docs/fwarsh.html.
// 
//  Solves the all-pairs shortest path problem using Floyd-Warshall algorithm
//  Input:  n - number of nodes
//          model - To get the cost information.
//  Output: retDist - shortest path dists(the answer)
//          retPred - predicate matrix, useful in reconstructing shortest routes
void fwarsh ( TrackModel *model, RoutePlanner *rp, int n ) {
	int  i, j, k; // Loop counters
	
	// Algorithm initialization
	for ( i = 0; i < n; i++ ) {
		for ( j = 0; j < n; j++ ) {
			
			// INT_MAX if no link; 0 if i == j
			rp->dists[i][j] = cost(model, i, j); 
			
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
}  //end of fwarsh()

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
	
	// Check each edge in O(3) time
	//debug ("cost: itr=%d type=%d\r\n", itr, model->nodes[i].type);
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

void outputPath (TrackModel *model, RoutePlanner *rp, int i, int j) {
	if ( rp->paths[i][j] == -1 ) {
		printf ("%s> ", model->nodes[i].name);
	}
	else {
		outputPath (model, rp, i, rp->paths[i][j]);
		outputPath (model, rp, rp->paths[i][j], j);
	}
}
