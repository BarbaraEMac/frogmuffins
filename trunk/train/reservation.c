/**
 * CS 452: TrainRes System
 * becmacdo
 * dgoc
 *
 * Used http://stackoverflow.com/questions/115426/algorithm-to-detect-intersection-of-two-rectangles.
 */
#define DEBUG 1

#include <debug.h>
#include <math.h>
#include <string.h>

#include "error.h"
#include "globals.h"
#include "model.h"
#include "reservation.h"
#include "syscalls.h"

#define NUM_RECTS		20

#define TRAIN_LEN		340 // mm
#define	ERROR_MARGIN	50	// mm
#define	TRAIN_EST		TRAIN_LEN + ERROR_MARGIN

// Private Stuff
// ----------------------------------------------------------------------------
typedef struct {
	int 		trainId;			// Train identifier
	int 		len;				// Number of rectangles
	Rectangle  	rects[NUM_RECTS];	// TrainRes rectangles
} TrainRes;

typedef struct {
	TrackModel	model;
	TrainRes 	entries[MAX_NUM_TRAINS];
} Reservation;

// Initialize the reservation structure
void	res_init		(Reservation *res);

// Make a reservation for a train
int 	res_makeTrainRes( Reservation *r, ResRequest *req );
// Build the rectangles for a reservations
int 	res_buildRects	( Reservation *r, TrainRes *d, ResRequest *req );
// Herlped to buildRects
int 	res_buildRectsHelper( Reservation *r, TrainRes *d, 
							  Node *n1, Node *n2, int distPast, 
							  int distLeft, int trainId );

// Test to see if a rectangle intersects with another reservation
int 	res_tryIntersect( Reservation *r, Rectangle *rect, int trainId );
// ----------------------------------------------------------------------------

void res_run () {
	int 		senderTid;
	Reservation res;			// Reservation system
	ResRequest	req;			// Reservation request
	ResReply	reply;			// Reservation reply

	FOREVER {
		// Receive from a TRAIN or the ROUTE PLANNER
		Receive( &senderTid, &req, sizeof(ResRequest) );

		switch( req.type ) {

			case TRAIN:
				reply.stopDist = res_makeTrainRes( &res, &req );
				
				break;

			case ROUTE_PLANNER:

				break;

			default:
				// TODO: error probably
				break;
		}

		// Reply to the sender
		Reply( senderTid, &reply, sizeof(ResReply) );
	}

	Exit(); // This will never be called.
}

void res_init(Reservation *res) {
	int  shellTid;
	char ch;
	int  err;
	int  track;
	int  i;

	// Get the model from the shell
	Receive(&shellTid, (char*)&ch, sizeof(ch));
	if ( ch != 'A' && ch != 'a' && ch != 'B' && ch != 'b' ) {
		err = INVALID_TRACK;
	}
	Reply  (shellTid,  (char*)&err, sizeof(int));

	// Parse the model for this track
	track = ( ch == 'A' || ch == 'a' ) ? TRACK_A : TRACK_B;
	parse_model( track, &res->model );

	// TODO: do more?
	for ( i = 0; i < MAX_NUM_TRAINS; i ++ ) {
		res->entries[i].len = 0;
	}

	RegisterAs ( RESERVATION_NAME );
}

int res_makeTrainRes( Reservation *r, ResRequest *req ) {
	TrainRes trRes = r->entries[ req->trainId ];
	int i;

	// Cancel the previous rectangles
	for ( i = 0; i < trRes.len; i ++ ) {
		// TODO: What if a train's way is now clear after removing these rects?
		rect_init( &trRes.rects[i] );
	}

	return res_buildRects( r, &trRes, req );
}

int res_buildRects( Reservation *r, TrainRes *d, ResRequest *req ) {
	Node *n = &r->model.nodes[ sIdxToIdx( req->sensor )];
	Edge *e = ((req->sensor % 2) == 0) ? n->se.ahead : n->se.behind; // next edge
	Node *nextNode = node_neighbour( n, e );


	// Assert that you are still on this edge.
	// TODO: This assertion might not be correct ..
	assert( req->distPast < e->distance );

	return req->stopDist - res_buildRectsHelper( r, d, n, nextNode, 
												 req->distPast, req->stopDist, 
												 req->trainId );
}

int res_buildRectsHelper( Reservation *r, TrainRes *d, Node *n1, Node *n2, 
						   int distPast, int distLeft, int trainId ) {
	
	Rectangle rect;
	Point	  p1 = { n1->x, n1->y };
	Point	  p2 = { n2->x, n2->y };
	Node 	  *next;
	Point	  start = ( distPast != 0 ) ? 
			  		  findPointOnLine( p1, p2, distPast ) : p1;
	int		  edgeDist = node_dist( n1, n2 );

	if ( distLeft - distPast <= edgeDist ) {
		// Make the rectangle & stop recursing
		rect = makeRectangle( start, 
							  findPointOnLine ( p1, p2, distLeft - distPast ));
	} else {

		// Construct the rectangle
		rect = makeRectangle( start, p2 );

		// If this rectangle intersects with another,
		if ( res_tryIntersect( r, &rect, trainId ) == INTERSECTION ) {
			return distLeft;
			// TODO:

		}

		// Store this rectangle
		d->rects[ d->len ] = rect;
		d->len ++;
		assert( d->len < NUM_RECTS );

		// Recurse on the next edges in all directions
		switch( n2->type ) {
			case NODE_SWITCH:
				
				if( (node_neighbour( n2, n2->sw.ahead[0] ) == n1 ) ||
					(node_neighbour( n2, n2->sw.ahead[1] ) == n1) ) {
					next = node_neighbour( n2, n2->sw.behind );

					res_buildRectsHelper( r, d, n2, next, 0, 
										  distLeft - edgeDist , trainId );
				
				} else { //behind
					next = node_neighbour( n2, n2->sw.ahead[0] ); 
					res_buildRectsHelper( r, d, n2, next, 0, 
										  distLeft - edgeDist, trainId );
					
					next = node_neighbour( n2, n2->sw.ahead[1] ); 
					res_buildRectsHelper( r, d, n2, next, 0, 
										  distLeft - edgeDist, trainId );
				}

				break;
			
			case NODE_SENSOR:
				if ( node_neighbour( n2, n2->se.ahead ) == n1 ) {
					next = node_neighbour( n2, n2->se.behind );
				} else {
					next = node_neighbour( n2, n2->se.ahead );
				}

				res_buildRectsHelper( r, d, n2, next, 0, 
										distLeft - edgeDist, trainId );
				break;
			case NODE_STOP:
				// Do nothing for a stop.
				break;
		}
	}
}

int res_tryIntersect( Reservation *r, Rectangle *rect, int trainId ) {
	int 		 i, j;
	TrainRes 	*trItr;
	Rectangle	*rectItr;

	for ( i = 0; i < MAX_NUM_TRAINS; i ++ ) {
		trItr = &r->entries[i];

		if( trItr->trainId == trainId ) {
			continue;
		}

		for ( j = 0; j < NUM_RECTS; j ++ ) {
			rectItr = &trItr->rects[j];

			if( rect_intersect( rect, rectItr ) != NO_INTERSECTION) {
			
				// TODO: return something more useful.
				// What is the maximum distance a train can go?
				return INTERSECTION;
			}
		}
	}
	return NO_INTERSECTION;
}
