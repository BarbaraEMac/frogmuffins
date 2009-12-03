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
#define NUM_TRAINS		6

#define TRAIN_LEN		340 // mm
#define	ERROR_MARGIN	50	// mm
#define	TRAIN_EST		TRAIN_LEN + ERROR_MARGIN

// Private Stuff
// ----------------------------------------------------------------------------
typedef struct {
	int 		trainId;			// Train identifier
	int			trainTid;			// Tid for this train
	int 		rectLen;			// Number of rectangles
	Rectangle  	rects[NUM_RECTS];	// TrainRes rectangles

	int 		idxsLen;			// Number of idxs
	int			idxs[NUM_IDXS];		// Reserved Node / Edge indexes
} TrainRes;

typedef struct {
	TrackModel	*model;
	TrainRes 	 entries  [NUM_TRAINS];
	TID			 courierTid;
} Reservation;

typedef struct {
	int dest;
	TID trainTid;
} ResCourierReply;

// Initialize the reservation structure
void	res_init			(Reservation *res);

// Cancel a reservation for a train
void 	res_cancelTrainRes	( Reservation *r, ResRequest *req );
// Make a reservation for a train
int 	res_makeTrainRes	( Reservation *r, ResRequest *req );

// Build the rectangles for a reservations
int 	res_buildRects		( Reservation *r, TrainRes *d, ResRequest *req );
// Herlped to buildRects
int 	res_buildRectsHelper( Reservation *r, TrainRes *d, 
							  Node *n1, Node *n2, int distPast, 
							  int distLeft, int trainId );

// Test to see if a rectangle intersects with another reservation
int 	res_checkIntersect	( Reservation *r, Rectangle *rect, int trainId );

void 	res_saveTrainTid	( Reservation *res, int trainId, TID tid );

// Given a trainId, return the corresponding array index
inline int 	mapTrainId   	(int trainId);
// Given an array index, return the corresponding train id
inline int 	mapIdxToTrainId (int idx);

// Train Reservation functions
inline void 	trRes_saveRect		( TrainRes *d, Rectangle rect );
inline void 	trRes_reserveNode	( TrainRes *d, Node *n );

// Courier
void 		resCourier_run	();
// ----------------------------------------------------------------------------

void res_run () {
	TID 		senderTid;
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

				// Store the train's tid for later use ...
				res_saveTrainTid( &res, req.trainId, senderTid );

				// Cancel the previous reservation for this train.
				res_cancelTrainRes( &res, &req );
				
				// Make the reservation for this train.
				reply.stopDist = res_makeTrainRes( &res, &req );
				
				break;

			case COURIER:
				// Hang out until we need it
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
	for ( i = 0; i < NUM_TRAINS; i ++ ) {
		res->entries[i].rectLen = 0;
		res->entries[i].idxsLen = 0;
		res->entries[i].trainId = mapIdxToTrainId( i );

		res->entries[i].trainTid = -1;
	}

	// Create the res->train courier.
	res->courierTid = Create( OTH_NOTIFIER_PRTY, &resCourier_run );

	// Register with the Name Server
	RegisterAs ( RESERVATION_NAME );
}

void res_cancelTrainRes( Reservation *r, ResRequest *req ) {
	
	TrainRes *trRes = &r->entries[ mapTrainId( req->trainId ) ];
	int i;
	// Unreserve all of the nodes and edges
	for ( i = 0; i < trRes->idxsLen; i ++ ) {
		debug( "res: Cancelling node %s(%d).\r\n", 
			   r->model->nodes[trRes->idxs[i]].name, trRes->idxs[i] );
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
}


int res_makeTrainRes( Reservation *r, ResRequest *req ) {
	debug( "res: Making a reservation. [id=%d, sensor=%d, past=%d, stop=%d]\r\n", 
			req->trainId, req->sensor, req->distPast, req->stopDist );

	int 	  ret;
	TrainRes *trRes = &r->entries[ mapTrainId( req->trainId ) ];

	ret = res_buildRects( r, trRes, req );

	debug ("RETURNING a safe distance of: %d\r\n", ret );

	return ret;
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
		// Save the rect
		trRes_saveRect( d, rect );

		return distLeft;

	} else {
		// Construct the rectangle
		rect = makeRectangle( start, p2 );

		// If this rectangle intersects with another,
		if ( res_checkIntersect( r, &rect, trainId ) == INTERSECTION ) {
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

int res_checkIntersect( Reservation *r, Rectangle *rect, int trainId ) {
	int 		 i, j;
	TrainRes 	*trItr;
	Rectangle	*rectItr;

	for ( i = 0; i < NUM_TRAINS; i ++ ) {
		trItr = &r->entries[i];

		if( trItr->trainId == trainId ) {
			continue;
		}

		debug("CHECKING INTERSECTION of this (%d) and (%d)\r\n", trainId, trItr->trainId);

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

void res_saveTrainTid( Reservation *res, int trainId, TID tid ) {
	int idx = mapTrainId( trainId );

	if ( res->entries[idx].trainTid != tid ) {
		res->entries[idx].trainTid = tid;
	}
}

// And the hardcoding begins!
inline int mapTrainId( int trainId ) {
	switch (trainId) {
		case 12:
			return 0;
		case 15:
			return 1;
		case 22:
			return 2;
		case 24:
			return 3;
		case 46:
			return 4;
		case 52:
			return 5;
	}
	// ERROR
	return 5;
}

inline int mapIdxToTrainId( int idx ) {
	switch( idx ) {
		case 0:
			return 12;
		case 1:
			return 15;
		case 2:
			return 22;
		case 3:
			return 24;
		case 4:
			return 46;
		case 5:
			return 52;
	}
	// ERROR
	return -1;
}

// ----------------------------------------------------------------------------
// --------------------------- Train Reservation ------------------------------
// ----------------------------------------------------------------------------
inline void trRes_saveRect( TrainRes *d, Rectangle rect ) {
	debug("saving Rectangle for %d\r\n", d->trainId);
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


// ----------------------------------------------------------------------------
// --------------------------------- Courier ----------------------------------
// ----------------------------------------------------------------------------
void resCourier_run( ) {
	int 				resTid = WhoIs( RESERVATION_NAME );
	TrainReq   			trReq;
	ResRequest			resReq;
	ResCourierReply		reply;

	resReq.type = COURIER;
	trReq.type  = DEST_UPDATE;
	trReq.mode	= DRIVE;

	FOREVER {
		// Send to the Reservation Server
		Send( resTid, &resReq, sizeof(ResRequest), &reply, sizeof(ResCourierReply));

		// Save the destination index
		trReq.dest = reply.dest;

		// Send to the train
		Send( reply.trainTid, &trReq, sizeof(TrainReq), 0, 0 );
	}

	Exit(); // This will never be called.
}
