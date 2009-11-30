/**
 * CS 452: TrainRes System
 * becmacdo
 * dgoc
 *
 * Used http://stackoverflow.com/questions/115426/algorithm-to-detect-intersection-of-two-rectangles.
 */
#define DEBUG 2

#include <debug.h>
#include <math.h>
#include <string.h>

#include "error.h"
#include "globals.h"
#include "model.h"
#include "reservation.h"
#include "servers.h"
#include "syscalls.h"
#include "train.h"

#define NUM_RECTS		20
#define NUM_IDXS		20

#define TRAIN_LEN		340 // mm
#define	ERROR_MARGIN	50	// mm
#define	TRAIN_EST		TRAIN_LEN + ERROR_MARGIN

// Private Stuff
// ----------------------------------------------------------------------------
typedef struct {
	int 		trainId;			// Train identifier
	int 		rectLen;			// Number of rectangles
	Rectangle  	rects[NUM_RECTS];	// TrainRes rectangles

	int 		idxsLen;			// Number of idxs
	int			idxs[NUM_IDXS];		// Reserved Node / Edge indexes
} TrainRes;

typedef struct {
	TrackModel	*model;
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
int 	res_checkIntersection( Reservation *r, Rectangle *rect, int trainId );

// Given a trainId, return the corresponding array index
int 	mapTrainId   		(int trainId);

// Train Reservation functions
inline void 	trRes_saveRect	( TrainRes *d, Rectangle rect );
inline void 	trRes_reserveNode( TrainRes *d, Node *n );

// ----------------------------------------------------------------------------

void res_run () {
	int 		senderTid;
	Reservation res;			// Reservation system
	ResRequest	req;			// Reservation request
	ResReply	reply;			// Reservation reply

	// Initialize the reservation system
	res_init( &res );
	
	FOREVER {
		// Receive from a TRAIN or the ROUTE PLANNER
		Receive( &senderTid, &req, sizeof(ResRequest) );

		// If the train has not located itself, do not reserve anything.
		if ( req.sensor == START_SENSOR ) {
			reply.stopDist = req.stopDist;
			req.type       = 0;
		}

		switch( req.type ) {

			case TRAIN_TASK:
				assert( req.stopDist > 0 );

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
	int  i;
	int  addr;
	int  rpTid;

	// Receive the address of the model from the Route Planner
	Receive( &rpTid, &addr, sizeof(int) );
	Reply( rpTid, 0, 0 );

	// Save this address as our model.
	// HACK: The Route Planner and Reservation System SHARE the same model.
	res->model = addr;

	// Clear each train reservation
	for ( i = 0; i < MAX_NUM_TRAINS; i ++ ) {
		res->entries[i].rectLen = 0;
		res->entries[i].idxsLen = 0;
	}

	// Register with the Name Server
	RegisterAs ( RESERVATION_NAME );
}

int res_makeTrainRes( Reservation *r, ResRequest *req ) {
	debug( "res: Making a reservation. [id=%d, sensor=%d, past=%d, stop=%d]\r\n", req->trainId, req->sensor, req->distPast, req->stopDist );

	TrainRes *trRes = &r->entries[ mapTrainId( req->trainId ) ];
	int i;

	// Unreserve all of the nodes and edges
	for ( i = 0; i < trRes->idxsLen; i ++ ) {
		debug("res: Cancelling node %s(%d).\r\n", r->model->nodes[trRes->idxs[i]].name, trRes->idxs[i]);
		r->model->nodes[ trRes->idxs[i]].reserved = 0;
		trRes->idxs[i] = 0;
	}
	trRes->idxsLen = 0;

	// Cancel the previous rectangles
	for ( i = 0; i < trRes->rectLen; i ++ ) {
		// TODO: What if a train's way is now clear after removing these rects?
		rect_init( &trRes->rects[i] );
	}
	trRes->rectLen = 0;

	return res_buildRects( r, trRes, req );
}

int res_buildRects( Reservation *r, TrainRes *d, ResRequest *req ) {
	Node *n = &r->model->nodes[ sIdxToIdx( req->sensor )];
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
	assert( n1 != n2 );

	debug("build Rects: n1=%s n2=%s past=%d left=%d id=%d trRes=%x\r\n", n1->name, n2->name, distPast, distLeft, trainId, d);
	debug("n1=(%d, %d) n2=(%d, %d)\r\n", n1->x, n1->y, n2->x, n2->y);

	Rectangle rect;
	Point	  p1 = { n1->x, n1->y };
	Point	  p2 = { n2->x, n2->y };
	Node 	 *next;
	Point	  start = ( distPast != 0 ) ? 
			  		  findPointOnLine( p1, p2, distPast ) : p1;
	Point	  nextP;
	int		  edgeDist = node_dist( n1, n2 );
	int		  pDist = pointDist( p1, p2 );

	debug("edge dist=%d pointDist = %d\r\n", edgeDist, pDist);

	// Only travel the distance required
	distLeft -= distPast;

	// Reserve the first node
	trRes_reserveNode( d, n1 );

	if ( distLeft <= edgeDist ) {
	  	
		nextP = findPointOnLine ( p1, p2, distLeft );
		debug("start=(%d, %d) next(%d, %d)\r\n", start.x, start.y, nextP.x, nextP.y);
//		Getc(WhoIs(SERIALIO2_NAME));
		
		// Make the rectangle & stop recursing
		rect = makeRectangle( start, nextP );

		return distLeft;

	} else {
		// Construct the rectangle
		rect = makeRectangle( start, p2 );

		// If this rectangle intersects with another,
		if ( res_checkIntersection( r, &rect, trainId ) == INTERSECTION ) {
			return distLeft;
		}
		
		// Remove the distance of the edge you just took
		distLeft -= edgeDist;

		// Save the rect
		trRes_saveRect( d, rect );
		
		// Recurse on the next edges in all directions
		switch( n2->type ) {
			case NODE_SWITCH:
				//debug ("next node is a switch\r\n");
				if( (node_neighbour( n2, n2->sw.ahead[0] ) == n1) ||
					(node_neighbour( n2, n2->sw.ahead[1] ) == n1) ) {
					next = node_neighbour( n2, n2->sw.behind );

				//	debug("recursing on %s\r\n", next->name);
					return res_buildRectsHelper( r, d, n2, next, 0, 
										  		 distLeft, trainId );
				
				} else { //behind
					next = node_neighbour( n2, n2->sw.ahead[0] ); 
				//	debug("recursing on %s\r\n", next->name);
					return res_buildRectsHelper( r, d, n2, next, 0, 
										  		 distLeft, trainId );
					
					next = node_neighbour( n2, n2->sw.ahead[1] ); 
				//	debug("recursing on %s\r\n", next->name);
					return res_buildRectsHelper( r, d, n2, next, 0, 
										  		 distLeft, trainId );
				}

				break;
			
			case NODE_SENSOR:
			//	debug("next node is a sensor\r\n");
				if ( node_neighbour( n2, n2->se.ahead ) == n1 ) {
					next = node_neighbour( n2, n2->se.behind );
				} else {
					next = node_neighbour( n2, n2->se.ahead );
				}

			//		debug("recursing on %s\r\n", next->name);
				return res_buildRectsHelper( r, d, n2, next, 0, 
											 distLeft, trainId );
				break;
			case NODE_STOP:

				return distLeft;
			//	debug("next node is a stop\r\n");
				// Do nothing for a stop.
				break;
		}
	}
}

int res_checkIntersection( Reservation *r, Rectangle *rect, int trainId ) {
	int 		 i, j;
	TrainRes 	*trItr;
	Rectangle	*rectItr;

	for ( i = 0; i < MAX_NUM_TRAINS; i ++ ) {
		trItr = &r->entries[i];

		if( trItr->trainId == trainId ) {
			continue;
		}

		for ( j = 0; j < trItr->rectLen; j ++ ) {
			rectItr = &trItr->rects[j];

			// If the boxes collide OR share an edge,
			// return there is an intersection
			if( rect_intersect( rect, rectItr ) != NO_INTERSECTION ) {
				debug ("%d rect intersects with this train's (%d).\r\n",
						trItr->trainId, trainId);
				// TODO: return something more useful.
				// What is the maximum distance a train can go?
				return INTERSECTION;
			}
		}
	}
	return NO_INTERSECTION;
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

// ----------------------------------------------------------------------------
// --------------------------- Train Reservation ------------------------------
// ----------------------------------------------------------------------------
inline void trRes_saveRect( TrainRes *d, Rectangle rect ) {
	// Store this rectangle
	d->rects[ d->rectLen ] = rect;
	d->rectLen ++;
	assert( d->rectLen < NUM_RECTS );
}

inline void trRes_reserveNode( TrainRes *d, Node *n ) {
	assert( n->reserved == 0 );

	// Reserve the corresponding node
	d->idxs[ d->idxsLen ] = n->idx;
	d->idxsLen ++;
	assert( d->idxsLen <= NUM_IDXS );

	n->reserved = 1;
	debug ("res: Reserving %s(%d).\r\n", n->name, n->idx);
}
