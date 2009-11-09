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
#include "model.h"
#include "requests.h"
#include "servers.h"
#include "train.h"

#define INT_MAX		0xFFFF


// Private Stuff
// ----------------------------------------------------------------------------

typedef struct {
	// what will this look like? How do we implement it?
} Reservation;


// ----------------------------------------------------------------------------

void routeplaner_run() {
	
	TrackModel *model;

	// Get the model from the first task
	// Send ();

	// Initialize shortest paths using model
	// Initialize track reservation system (nothing is reserved)
	
	// FOREVER 
	// Receive from client - current location, destination, speed?, current time?
	// Determine shortest path (observing blockages / reservations)
	// Return the speed and direction? Next landmark? (does train even know the track topology?)


}

void routeplaner_init() {

}


// Floyd-Warshall algorithm from http://www.joshuarobinson.net/docs/fwarsh.html.
// 
//  Solves the all-pairs shortest path problem using Floyd-Warshall algorithm
//  Input:  n, number of nodes
//          connectivity matrix cost, where 0 means disconnected
//             and distances are all positive. 
//             Array of ints of length (n*n).
//  Output: retDist - shortest path distances(the answer)
//          retPred - predicate matrix, useful in reconstructing shortest routes
void fwarsh ( int n, int *cost, int **retDist, int **retPred ) {
	
	int *dist = *retDist;		// Copy over to make pointer stuff easier
	int *pred = *retPred;
	int  i,j,k; 				// Loop counters

	// Initialize data structures
	memoryset(dist, 0, sizeof(int)*n*n);
	memoryset(pred, 0, sizeof(int)*n*n);

	// Algorithm initialization
	for (i=0; i < n; i++) {
		for (j=0; j < n; j++) {
			if (cost[i*n+j] != 0)
				dist[i*n+j] = cost[i*n + j];
			else
				dist[i*n+j] = INT_MAX; //disconnected

			if (i==j)  //diagonal case
				dist[i*n+j] = 0;

			if ((dist[i*n + j] > 0) && (dist[i*n+j] < INT_MAX))
				pred[i*n+j] = i;
		}
	}
 
  	// Main loop of the algorithm
	for (k=0; k < n; k++) {
		for (i=0; i < n; i++) {
			for (j=0; j < n; j++) {
				if (dist[i*n+j] > (dist[i*n+k] + dist[k*n+j])) {
	  				dist[i*n+j] = dist[i*n+k] + dist[k*n+j];
	  				pred[i*n+j] = k; 
	  				//printf("updated entry %d,%d with %d\n", i,j, dist[i*n+j]);
				}
      		}
    	}
  	}

  	/* 
	// Print out the results table of shortest distances
  	for (i=0; i < n; i++) {
    	for (j=0; j < n; j++)
      		printf("%g \n", dist[i*n+j]);
    }
	*/

}  //end of fwarsh()

