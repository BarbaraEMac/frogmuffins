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
#include "routeplanner.h"
#include "servers.h"
#include "syscalls.h"
#include "train.h"

#define NUM_RECTS		20
#define NUM_IDXS		20
#define NUM_TRAINS		6

#define TRAIN_LEN		340 // mm
#define	ERROR_MARGIN	60	// mm
#define	TRAIN_EST		TRAIN_LEN + ERROR_MARGIN

#define SMALLEST_RECT	5

// Private Stuff
// ----------------------------------------------------------------------------
typedef struct {
	int 		trainId;			// Train identifier
	int			trainTid;			// Tid for this train

	bool		idle;				// 1 if this train is idle. 0, otherwise.

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
void		res_init			(Reservation *res);

// Free a stuck train! No guarantees trains will go back to where they were ... yet
void		res_freeTrain		( Reservation *r, ResRequest *req );
// Simulate a move in ths "fake" reservation system
void 		res_simulateStep	( Reservation *r, TrainRes *toMove );
// Give an idle train a destination to make it move
void 		res_moveTrain		( Reservation *r, TrainRes *toMove, int dest );
// Test to see if a rectangle intersects with another reservation
int 		res_checkIntersect	( Reservation *r, Rectangle *rect, int trainId );

// Given a trainId, return the corresponding array index
inline int 	mapTrainId   		(int trainId);
// Given an array index, return the corresponding train id
inline int 	mapIdxToTrainId 	(int idx);

// Train Reservation functions
inline void	trRes_saveRect		( TrainRes *d, Rectangle rect );
inline void	trRes_reserveNode	( TrainRes *d, Node *n );
// Create a reserved bubble around a train
void	 	trRes_encircle		( TrainRes *trRes, Reservation *res, ResRequest *req );
// Cancel a reservation for a train
void 		trRes_cancel		( TrainRes *res, Reservation *r );
// Make a reservation for a train
int 		trRes_make			( TrainRes *trRes, Reservation *r, ResRequest *req );
// Build the rectangles for a reservations
int 		trRes_buildRects	( TrainRes *trRes, Reservation *r, ResRequest *req );
// Herlped to buildRects
int 		trRes_buildRectsH	( TrainRes *trRes, Reservation *r, Node *n1, Node *n2, 
								  int distPast, int distLeft, int trainId );
// Courier
void 		resCourier_run		();
// ----------------------------------------------------------------------------

void res_run () {
	TID 		senderTid;
	Reservation res;			// Reservation system
	ResRequest	req;			// Reservation request
	ResReply	reply;			// Reservation reply
	TrainRes   *trRes;
	int			distLeft;

	// Initialize the reservation system
	res_init( &res );
	
	FOREVER {
		// Receive from a TRAIN or the ROUTE PLANNER
		Receive( &senderTid, &req, sizeof(ResRequest) );
		
		// If the train has not located itself, do not reserve anything.
		if ( req.sensor == START_SENSOR ) {
			reply.stopDist = req.stopDist;
			req.type       = 0;
			
			// Reply to the sender train
			Reply( senderTid, &reply, sizeof(ResReply) );
		}

		switch( req.type ) {

			case TRAIN_MAKE:
				assert( req.stopDist >= 0 );
				assert( req.totalDist >= 0 );
		
				// Store the train's tid for later use ...
				trRes = &res.entries[ mapTrainId( req.trainId ) ];
				trRes->trainTid = senderTid;

				// Cancel the previous reservation for this train.
				trRes_cancel( trRes, &res );
				
				if ( req.totalDist == 0 ) {
					trRes_encircle( trRes, &res, &req );
					reply.stopDist = 0;
				} else {

					// Make the reservation for this train.
					distLeft = trRes_make( trRes, &res, &req );
					debug( "stop dist= %d distLeft=%d total=%d other=%d\r\n", 
						  req.stopDist, distLeft, req.totalDist, req.stopDist - distLeft );
					assert( distLeft >= 0 );
					assert( req.stopDist >= distLeft );
					
					reply.stopDist = (distLeft <= 20) ? 
						min(req.totalDist - req.distPast, req.stopDist - distLeft) : 
						req.stopDist - distLeft;

					assert( reply.stopDist >= 0 );
					debug("train can safely travel %d\r\n", reply.stopDist );
				}

				// Reply to the sender train
				Reply( senderTid, &reply, sizeof(ResReply) );

				break;

			case TRAIN_STUCK:
				// Free that poor train
				res_freeTrain( &res, &req );

				// Either:
				// 1. Other blocking trains are starting to move.
				// 2. The other blocking trains were not idle.
				Reply( senderTid, &reply, sizeof(ResReply) );

				break;

			case COURIER:
				// Hang out until we need it
				break;

			default:
				// TODO: error probably
				break;
		}
	}

	Exit(); // This will never be called.
}

void res_init(Reservation *res) {
	int  i;
	int  addr;
	int  rpTid;

	// Receive the address of the model from the Route Planner
	Receive( &rpTid, &addr, sizeof(int) );

	// Save this address as our model.
	// HACK: The Route Planner and Reservation System SHARE the same model.
	res->model = addr;

	// Clear each train reservation
	for ( i = 0; i < NUM_TRAINS; i ++ ) {
		res->entries[i].trainId = mapIdxToTrainId( i );
		res->entries[i].trainTid = -1;
		res->entries[i].idle = false;
		
		res->entries[i].rectLen = 0;
		res->entries[i].idxsLen = 0;
	}

	// Create the res->train courier.
	res->courierTid = Create( OTH_NOTIFIER_PRTY, &resCourier_run );

	// Register with the Name Server
	RegisterAs ( RESERVATION_NAME );
	
	// Reply to the shell after registering
	Reply( rpTid, 0, 0 );
}

void res_freeTrain( Reservation *r, ResRequest *req ) {
	int   	 i;
	Node 	*n;
	int		 path[r->model->num_nodes];
	TrainRes steps[NUM_TRAINS*5];
	int 	 stepsLen  = 0;
	int	 	 newSteps  = 0;
	int		 prevSteps = 0;
	int		 trainDests[NUM_TRAINS];
	int 	 idx;

	// Create a reservation system and model for this simulation
	Reservation simRes   = *r;
	TrackModel  simModel = *r->model;
	simRes.model = &simModel;

	// Init to garbage
	for ( i = 0; i < NUM_TRAINS; i ++ ) {
		trainDests[i] = -1;
	}

	// Create the shortest path
	// TODO: Call FW once and use those results
	dijkstra( &simModel, req->sensor, req->dest, path );

	FOREVER {
		// Get all of the train reservations blocking the path
		newSteps  = 0;
		prevSteps = stepsLen;
		for( i = 0; path[i] != req->dest; i ++ ) {
			n = &simModel.nodes[ path[i] ];
			
			if ( (n->reserved == true) && 
				 (simRes.entries[mapTrainId(n->reserver)].idle == true) ) {
				steps[stepsLen++] = simRes.entries[ mapTrainId(n->reserver) ];
				newSteps ++;
			}
		}

		// Once we don't need anymore steps, stop!
		if ( newSteps == 0 ) {
			break;
		}

		// "Move" the blocking trains along the track to a new location
		// Note that the trains only move forwards and do not flip switches
		for ( i = prevSteps; i < stepsLen; i ++ ) {
			res_simulateStep( r, &steps[i] );
		}
	}

	// Now that we have an ordered list that clears the path,
	// move the trains!
	// Start at the end to pick the furthest destination
	for ( i = stepsLen - 1; i >= 0; i -- ) {
		idx = mapTrainId(steps[i].trainId);

		// Only give the train 1 destination
		if ( trainDests[idx] == -1 ) {
			res_moveTrain( r, &steps[i], steps[i].idxs[steps[i].idxsLen - 1] );
			trainDests[idx] = steps[i].idxs[steps[i].idxsLen - 1];
		}
	}


	// TODO: How to make the train go back to the start?
}

void res_simulateStep( Reservation *r, TrainRes *step ) {
	ResRequest req;
	int 	   dest = step->idxs[ step->idxsLen - 1 ];

	// Cancel this reservation
	trRes_cancel( step, r );
	
	req.sensor   = dest; // TODO: not a sensor?
	req.stopDist = 400; // length of path to get out of the way?
	req.distPast = 0; 

	trRes_make( step, r, &req );

	// Make the train appear idle
	step->idle = true;
}

void res_moveTrain( Reservation *r, TrainRes *d, int dest ) {
	assert( d->idle == true );

	ResCourierReply reply;

	reply.dest 	   = dest;
	reply.trainTid = d->trainTid;

	Reply( r->courierTid, &reply, sizeof(ResCourierReply) );
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

		for ( j = 0; j < trItr->rectLen; j ++ ) {
			rectItr = &trItr->rects[j];

			// If the boxes collide OR share an edge,
			// return there is an intersection
			if( rect_intersect( rect, rectItr ) != NO_INTERSECTION ) {
				printf ("%d rect intersects with this train's (%d).\r\n",
						trItr->trainId, trainId);

				// Move this train out of our way incase we come here
				if ( trItr->idle == true ) {
					printf ("Telling %d to move!\r\n", trItr->trainId);
					res_moveTrain( r, trItr, 69 /*DE*/); // TODO: not pick a random dest ....
				}

				// TODO: return something more useful.
				// What is the maximum distance a train can go?
				return INTERSECTION;
			}
		}
	}
	return NO_INTERSECTION;
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
	// Store this rectangle
	d->rects[ d->rectLen ] = rect;
	d->rectLen ++;
	assert( d->rectLen < NUM_RECTS );
}

inline void trRes_reserveNode( TrainRes *d, Node *n ) {
	assert( (n->reserved == 0) || (n->reserved == 1 && n->reserver == d->trainId) );

	// Reserve the corresponding node
	d->idxs[ d->idxsLen ] = n->idx;
	d->idxsLen ++;
	assert( d->idxsLen <= NUM_IDXS );

	n->reserved = 1;
	n->reserver = d->trainId;
	printf ("%d: Reserving %s(%d).\r\n", d->trainId, n->name, n->idx);
}

void trRes_encircle( TrainRes *trRes, Reservation *res, ResRequest *req ) {
	if( trRes->idle == true ) return;
		
	//	printf( "\r\nEncircle [id=%d, sensor=%d, past=%d, stop=%d total=%d res=%x]\r\n", 
//			req->trainId, req->sensor, req->distPast, 
//			req->stopDist, req->totalDist, trRes );

	// Only encircle stopped trains
	assert( req->totalDist == 0 );

	// This train has stopped
	trRes->idle = true;

	// Build the rectangles ahead
//	req->stopDist = (TRAIN_LEN / 1) + ERROR_MARGIN;
	req->stopDist = TRAIN_LEN;
	trRes_buildRects( trRes, res, req );

	// Flip the sensor
	req->sensor ^= 1;
	
	// Build the rectangles behind
	//req->stopDist = (TRAIN_LEN / 2) + ERROR_MARGIN;
	req->stopDist = TRAIN_LEN;
	trRes_buildRects( trRes, res, req );
}

void trRes_cancel( TrainRes *trRes, Reservation *r ) {
	int i;
	
	// Unreserve all of the nodes and edges
	for ( i = 0; i < trRes->idxsLen; i ++ ) {
		printf( "res: Cancelling node %s(%d).\r\n", 
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

int trRes_make( TrainRes *trRes, Reservation *r, ResRequest *req ) {
//	printf( "res: Making a reservation. [id=%d, sensor=%d, past=%d, stop=%d total=%d]\r\n", 
//			req->trainId, req->sensor, req->distPast, req->stopDist, req->totalDist );

	int distLeft;
	// Build the rectangles long the path
	distLeft = trRes_buildRects( trRes, r, req );

	// Set this train in motion so that we don't accidentally tell it to move.
	if ( distLeft != 0 ) {
		trRes->idle = false;
	}

	debug ("RETURNING a distLeft of: %d\r\n", distLeft );
	if ( distLeft < 0 ) {
		assert( withinEpsilon( distLeft, 0, 10 ) );
		distLeft = 0;
	}

	return distLeft;
}

int trRes_buildRects( TrainRes *trRes, Reservation *r, ResRequest *req ) {
	Node *n = &r->model->nodes[ sIdxToIdx( req->sensor )];
	Edge *e = ((req->sensor % 2) == 0) ? n->se.ahead : n->se.behind; // next edge
	Node *nextNode = node_neighbour( n, e );
	int	  distLeft;

	// Assert that you are still on this edge.
	// TODO: This assertion might not be correct ..
//	assert( req->distPast < e->distance );
	if ( req->distPast >= e->distance ) {
		req->distPast = 0;
	}

	// Make rectangles until the end of the stopping distance
	// OR your rectangle collides with another's
	distLeft = trRes_buildRectsH( trRes, r, n, nextNode, req->distPast, 
								  req->stopDist, req->trainId );

	return distLeft;
}

int trRes_buildRectsH( TrainRes *trRes, Reservation *r, Node *n1, Node *n2, 
						  int distPast, int distLeft, int trainId ) {
	assert( n1 != n2 );

	debug( "build Rects: n1=%s n2=%s past=%d left=%d id=%d\r\n", 
		   n1->name, n2->name, distPast, distLeft, trainId );
	debug("n1=(%d, %d) n2=(%d, %d)\r\n", n1->x, n1->y, n2->x, n2->y);

	Rectangle rect;
	Point	  p1 = { n1->x, n1->y };
	Point	  p2 = { n2->x, n2->y };
	Node 	 *next;
	int		  edgeDist = node_dist( n1, n2 );
	int		  pDist    = pointDist( p1, p2 );

	debug("edge dist=%d pointDist=%d distLeft=%d\r\n", edgeDist, pDist, distLeft);

	// Reserve the first node
	trRes_reserveNode( trRes, n1 );

	// It is really hard to make exact rectangles of len < 5
	if ( distLeft <= SMALLEST_RECT ) {
		return distLeft;
	}
	
	if ( distPast > 0 ) {
		p1 = findPointOnLine( p1, p2, distPast );
	}

	if ( (distLeft + distPast) <= edgeDist ) {
		Point t = findPointOnLine( p1, p2, distLeft );
		
		// Make the rectangle & stop recursing
		rect = makeRectangle( p1, t );
		
		// If this rectangle intersects with another,
		if ( res_checkIntersect( r, &rect, trainId ) == INTERSECTION ) {
			return distLeft;
		}
		
		// Save the rect
		trRes_saveRect( trRes, rect );
		
		return 0;// rect.len is srraightline dist (distLeft - rect.len); // Probably should return ~0

	} else {
		// Construct the rectangle
		rect = makeRectangle( p1, p2 );

		// If this rectangle intersects with another,
		if ( res_checkIntersect( r, &rect, trainId ) == INTERSECTION ) {
			return distLeft;
		}
		
		// Remove the distance of the edge you just took
		assert( (distLeft+distPast) > edgeDist );
		distLeft -= (edgeDist - distPast);
		assert( distLeft > 0 );

		// Save the rect
		trRes_saveRect( trRes, rect );
		
		// Recurse on the next edges in all directions
		switch( n2->type ) {
			case NODE_SWITCH:
				//debug ("next node is a switch\r\n");
				if( (node_neighbour( n2, n2->sw.ahead[0] ) == n1) ||
					(node_neighbour( n2, n2->sw.ahead[1] ) == n1) ) {
					next = node_neighbour( n2, n2->sw.behind );

				//	debug("recursing on %s\r\n", next->name);
					return trRes_buildRectsH( trRes, r, n2, next, 0, 
										  		 distLeft, trainId );
				
				} else { //behind
					next = node_neighbour( n2, n2->sw.ahead[0] ); 
				//	debug("recursing on %s\r\n", next->name);
					return trRes_buildRectsH( trRes, r, n2, next, 0, 
										  		 distLeft, trainId );
					
					next = node_neighbour( n2, n2->sw.ahead[1] ); 
				//	debug("recursing on %s\r\n", next->name);
					return trRes_buildRectsH( trRes, r, n2, next, 0, 
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
				return trRes_buildRectsH( trRes, r, n2, next, 0, 
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

// ----------------------------------------------------------------------------
// --------------------------------- Courier ----------------------------------
// ----------------------------------------------------------------------------
void resCourier_run( ) {
	int 				resTid = MyParentTid();
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
