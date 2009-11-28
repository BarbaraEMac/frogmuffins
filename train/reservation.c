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
#include "reservation.h"
#include "train.h"

#define NUM_RECTS		20

#define WIDTH			40 	// mm
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

void res_init(Reservation *res);

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

// ahead[0] = straight
// ahead[1] = curved
inline SwitchDir getSwitchDir (Node *sw, Node *next) {
	Edge *e = &sw->sw.ahead[0];

	return ( e->dest == next->idx ) ? SWITCH_STRAIGHT : SWITCH_CURVED;
}

int oppositeSensorId (int sensorIdx) {
//	debug ("opposite to %d is %d\r\n", sensorIdx, (sensorIdx ^1));
	return (sensorIdx ^ 1);
}


// ----------------------------------------------------------------------------

void res_run () {
	Reservation res;
	TrainReq 	trReq;
	ReqReq		req;			// Reservation request
	TrainRes		reserve;

	FOREVER {
		// TODO: Need couriers
		Receive( &senderTid, &req, sizeof(ResRequest) );

		switch( req.type ) {

			case TRAIN:
				res_makeTrainRes( &res, &req );
				
				break;

			case ROUTE_PLANNER:

				break;

			default:
				// TODO: error probably
				break;
		}
	}

	Exit(); // This will never be called.
}

void res_init(Reservation *res) {
	int  shellTid;
	char ch;
	int  err;
	int  track;


	// Get the model from the shell
	Receive(&shellTid, (char*)&ch, sizeof(ch));
	if ( ch != 'A' && ch != 'a' && ch != 'B' && ch != 'b' ) {
		err = INVALID_TRACK;
	}
	Reply  (shellTid,  (char*)&err, sizeof(int));

	// Parse the model for this track
	track = ( ch == 'A' || ch == 'a' ) ? TRACK_A : TRACK_B;
	parse_model( track, &res->model );


	RegisterAs ( RESERVATION_NAME );
}

void res_makeTrainRes( Reservation *res, ResRequest *req ) {
	TrainRes r = res->entries[ req->trainId ];
	int i;
	int x, y;		// Current location co-ords


	// Cancel the previous rectangles
	for ( i = 0; i < r->len; i ++ ) {
		rect_clear( &r->rect[i] );
	}


}

void res_buildRects( Reservation *r, TrainRes *d, ResRequest *req ) {
	Node *n = &r->model.nodes[ sIdxToIdx( req->sensor )];
	Edge *e = ((sensor % 2) == 0) ? &n->se.ahead : &n->se.behind; // next edge
	Node *nextNode = &req->model.nodes[ e->dest ];


	// Assert that you are still on this edge.
	// TODO: This assertion might not be correct ..
	assert( req->distPast < e->distance );

	res_buildRectsHelper( r, d, n, nextNode, req->distPast,
						  req->stopDist - e->distance );
}

void res_buildRectsHelper( Reservation *r, TrainRes *d, Node *n1, Node *n2, 
						   int distPast, int distLeft ) {
	
	Rectangle rect;
	Point	  p1 = { n1.x, n1.y };
	Point	  p2 = { n2.x, n2.y };
	Node 	  next;
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
		if ( res_tryIntersect( r, rect ) == 1 ) {

			// TODO:

		}

		// Store this rectangle
		d->rects[ d->len ] = rect;
		d->len ++;
		assert( d->len < NUM_RECTS );

		// Recurse on the next edges in all directions
		switch( n2->type ) {
			case NODE_SWITCH:
				
				if( (node_neighbour( n2, n2->sw.ahead[0] ) == n1.idx ) ||
					(node_neighbour( n2, n2->sw.ahead[1] ) == n1.idx) ) {
					next = r->model.nodes[node_neighbour( n2, n2->sw.behind)];

					res_buildRectsHelper( r, d, n2, next, 0, 
										  distLeft - edgeDist );
				
				} else { //behind
					next = node_neighbour( n2, n2->sw.ahead[0] ); 
					res_buildRectsHelper( r, d, n2, next, 0, 
										  distLeft - edgeDist );
					
					next = node_neighbour( n2, n2->sw.ahead[1] ); 
					res_buildRectsHelper( r, d, n2, next, 0, 
										  distLeft - edgeDist );
				}

				break;
			
			case NODE_SENSOR:
					next = node_neighbour( n2, n2->se.ahead ) == n1.idx ? 
						   n2->se.behind : n2->se.ahead;

					res_buildRectsHelper( r, d, n2, next, 0, 
										  distLeft - edgeDist );
				}

				break;
			case NODE_STOP:
				// Do nothing for a stop.
				break;
		}
	}
}

int res_tryIntersect( Reservation *r, Rectangle *rect, int trainId ) {
	int 		 i;
	TrainRes 	*trItr;
	Rectangle	*rectItr;

	for ( i = 0; i < MAX_NUM_TRAINS; i ++ ) {
		trItr = &r->entries[i];

		if( trItr->id == trainId ) {
			continue;
		}

		for ( j = 0; j < NUM_RECTS; j ++ ) {
			rectItr = &trItr->rects[j];

			if( intersect( rect, rectItr ) != NO_INTERSECTION) {
			
				// TODO: return something more useful.
				// What is the maximum distance a train can go?
				return 1;
			}
		}
	}
	return NO_INTERSECTION;
}

int node_dist( Node *n1, Node *n2 ) {
	int i;

	for ( i = 0; i < n1->type; i ++ ) {
		if ( n1->edges[i]->dest == n2.idx ) {
			return n1.edges[i]
		}
	}

	return INT_MAX;
}
