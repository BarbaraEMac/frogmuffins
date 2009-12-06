/**
 * Math Library
 */

#define DEBUG 1 

#include "debug.h"
#include "math.h"
#include "servers.h"
#include "string.h"

#define TRAIN_WIDTH			40 	// mm
#define EPSILON 			3

int
ctz( int x ) {
	if ( x == 0 ) return 32;
	int n = 0;
	int mask = 0xFFFF;
	if ( (x & mask) == 0 ) { n += 16; }
	mask = 0xFF << n;
	if ( (x & mask) == 0 ) { n += 8; }
	mask = 0xF << n;
	if ( (x & mask) == 0 ) { n += 4; }
	mask = 0x3 << n;
	if ( (x & mask) == 0 ) { n += 2; }
	mask = 0x1 << n;
	if ( (x & mask) == 0 ) { n += 1; }
	return n;
}

// The following function was copied from wikipedia
int
log_2(unsigned int x) {
  int r = 0;
  while ((x >> r) != 0) {
    r++;
  }
  return r-1; // returns -1 for x==0, floor(log2(x)) otherwise
}


int
abs ( int n ) {
	return (n > 0) ? n : -n ;
}

unsigned long
isqrt(x)
unsigned long x;
{
    register unsigned long op, res, one;

    op = x;
    res = 0;

    /* "one" starts at the highest power of four <= than the argument. */
    one = 1 << 30;  /* second-to-top bit set */
    while (one > op) one >>= 2;

    while (one != 0) {
        if (op >= res + one) {
            op = op - (res + one);
            res = res +  (one << 1);  // <-- faster than 2 * one
        }
        res >>= 1;
        one >>= 2;
    }
    return(res);
}

inline int sign ( int val ) {
	if ( val == 0 ) return ZERO;

	return (val < 0) ? NEG : POS;
}

// ----------------------------------------------------------------------------
// --------------------------- Geometry Stuff ---------------------------------
// ----------------------------------------------------------------------------
// Find the coord, along the line p1->p2, that is len away from p1.
Point findPointOnLine ( Point p1, Point p2, int len ) {
	debug ("findPointOnLine: (%d, %d) and (%d, %d) at len=%d pointDist=%d\r\n", p1.x,p1.y, p2.x,p2.y, len, pointDist(p1,p2));
	assert( !pointEqual(p1, p2) );
	
	if ( len == 0 ) {
		return p1;
	} else if ( len >= pointDist( p1, p2 ) ) {
		return p2;
	}

	Point mid  = midpoint( p1, p2 );
	int   dist = pointDist( p1, mid );

	if ( abs( len - dist) <= EPSILON ) {
		return mid;
	} else if ( len < dist ) {
		return findPointOnLine( p1, mid, len );
	} else {
		return findPointOnLine( mid, p2, len - dist );
	}
}

inline int pointDist( Point p1, Point p2 ) {
	int xDiff = p2.x - p1.x;
	int yDiff = p2.y - p1.y;

	return isqrt( xDiff*xDiff + yDiff*yDiff );
}

inline Point midpoint( Point p1, Point p2 ) {
	Point ret;

	ret.x = p1.x + p2.x;
	ret.y = p1.y + p2.y;

	ret.x /= 2;
	ret.y /= 2;

	return ret;
}

inline int slope( Point p1, Point p2 ) {
	int xDiff = p2.x - p1.x;
	int yDiff = p2.y - p1.y;

	return yDiff / xDiff;
}

inline int pointEqual( Point p1, Point p2 ) {
	return ( p1.x == p2.x ) && ( p1.y == p2.y );
}

// --------------------------- Vector -----------------------------------------
inline Vector vect_make( Point p1, Point p2 ) {
	return vect_sub( p2, p1 );
}

inline int vect_len( Vector v ) {
	return isqrt( v.x*v.x + v.y*v.y );
}

inline Vector vect_add( Vector v1, Vector v2 ) {
	Vector ret;

	ret.x = v1.x + v2.x;
	ret.y = v1.y + v2.y;

	return ret;
}

inline Vector vect_sub( Vector v1, Vector v2 ) {
	Vector ret;

	ret.x = v1.x - v2.x;
	ret.y = v1.y - v2.y;

	return ret;
}

inline int vect_dotProduct( Vector v1, Vector v2 ) {
	return (v1.x * v2.x) + (v1.y * v2.y);
}

// ------------------------ Rectangle -----------------------------------------
Rectangle makeRectangle( Point p1, Point p2 ) {
	debug("make from (%d, %d) to (%d, %d)\r\n", p1.x, p1.y, p2.x, p2.y);
	Rectangle rect;
	Vector 	  rectVect = vect_make( p1, p2 );
	Vector	  perp 	   = { -rectVect.y, rectVect.x };
	int    	  len  	   = vect_len ( perp );

//	debug("rectVect=(%d, %d) Perp=(%d, %d)\r\n", rectVect.x, rectVect.y, perp.x, perp.y);

	perp.x *= (TRAIN_WIDTH / 2);
	perp.y *= (TRAIN_WIDTH / 2);
	perp.x /= len;
	perp.y /= len;

//	debug("perp:(%d, %d)\r\n", perp.x, perp.y);

	rect.p[0] = vect_add( perp, p1 );
	rect.p[3] = vect_add( perp, p2 );

	perp.x = -perp.x;
	perp.y = -perp.y;

	rect.p[1] = vect_add( perp, p1 );
	rect.p[2] = vect_add( perp, p2);
	
	rect.len = vect_len( rectVect );
	
	int i;
	debug ("made a rect of len:%d\r\n", rect.len);
	for ( i = 0; i < 4; i ++ ) {
		if ( rect.p[i].x < 0 ) rect.p[i].x = 0;
		if ( rect.p[i].y < 0 ) rect.p[i].y = 0;

//		debug("corner:%d (%d, %d)\r\n", i, rect.p[i].x, rect.p[i].y);
	}
	return rect;
}

inline void rect_init( Rectangle *rect ) {
	int i;
	for ( i = 0; i < 4; i ++ ) {
		rect->p[i].x = NO_POINT;
		rect->p[i].y = NO_POINT;
	}
	rect->len = 0;
}

int rect_intersect( Rectangle *r1, Rectangle *r2 ) {
	int i;

	// For each edge made by the corners of r1,
	for ( i = 0; i < 4; i ++ ) {
		if ( rect_intersectH( r1, r2, i ) == NO_INTERSECTION ) {
			return NO_INTERSECTION;
		}
	}
		
	for ( i = 0; i < 4; i ++ ) {
		if ( rect_intersectH( r2, r1, i ) == NO_INTERSECTION ) {
			return NO_INTERSECTION;
		}
	}

	return INTERSECTION;
}

int rect_intersectH( Rectangle *r1, Rectangle *r2, int q ) {
	assert ( q <= 3 );

	int i;
	i = (q+1) % 4;
	Vector edge = vect_make( r1->p[q], r1->p[i] );
	Vector perp = { -edge.y, edge.x };
	int side1, side2;
	int r1Side;

	// Determine the side of the other 2 points of R1
	i = (q+2) % 4;
	side1 = sign( vect_dotProduct( perp, vect_make( r1->p[q], r1->p[i] )));

	i = (q+3) % 4;
	side2 = sign( vect_dotProduct( perp, vect_make( r1->p[q], r1->p[i] )));

	assert( side1 == side2 );
	
	// Store the side of the first rectangle
	r1Side = side1;

	// Determine the side of each point in the second rect
	for ( i = 0; i < 4; i ++ ) {
		side1 = sign( vect_dotProduct( perp, vect_make( r1->p[q], r2->p[i]) ) );

		if ( side1 == r1Side ) {
			//printf ("r1edge: (%d, %d) r1side=%d r2edge:(%d, %d) r2side=%d\r\n", 
			//		 r1->p[q].x, r1->p[q].y, r1Side, 
			//		 r2->p[i].x, r2->p[i].y, side1 ); 

			return INTERSECTION;
		}
		if ( side1 == r1Side ) { return INTERSECTION; }
	}

	return NO_INTERSECTION;
}

