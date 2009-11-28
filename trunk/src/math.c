/**
 * Math Library
 */

#include "math.h"

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

int sign ( int val ) {
	if ( val == 0 ) return 0;

	return (val < 0) ? NEG : POS;
}

// ----------------------------------------------------------------------------
// --------------------------- Geometry Stuff ---------------------------------
// ----------------------------------------------------------------------------
// Find the coord, along the line p1->p2, that is len away from p1.
Point findPointOnLine ( Point p1, Point p2, int len ) {
	Point mid  = midpoint( p1, p2 );
	int   dist = pointDist( p1, mid );

	if ( len == dist ) {
		return mid;
	}
	else if ( len < dist ) {
		return findPointOnLine( p1, mid, len );
	} else {
		return findPointOnLine( mid, p2, len - dist );
	}
}

int pointDist( Point p1, Point p2 ) {
	int xDiff = p2.x - p1.x;
	int yDiff = p2.y - p1.y;

	return isqrt( xDiff*xDiff + yDiff*yDiff );
}

Point midpoint( Point p1, Point p2 ) {
	Point ret;

	ret.x = p1.x + p2.x;
	ret.y = p1.y + p2.y;

	ret.x /= 2;
	ret.y /= 2;

	return ret;
}

int slope( Point p1, Point p2 ) {
	int xDiff = p2.x - p1.x;
	int yDiff = p2.y - p1.y;

	return yDiff / xDiff;
}

// --------------------------- Vector -----------------------------------------
Vector makeVector( Point p1, Point p2 ) {
	Vector ret;

	ret.x = p2.x - p1.x;
	ret.y = p2.y - p1.y;

	return ret;
}

int vectorLen( Vector v ) {
	return isqrt( v.x*v.x + v.y*v.y );
}

Vector vectorAdd( Vector v1, Vector v2 ) {
	Vector ret;

	ret.x = v1.x + v2.x;
	ret.y = v1.y + v2.y;

	return ret;
}

// ------------------------ Rectangle -----------------------------------------
Rectangle makeRectangle( Point p1, Point p2 ) {
	Rectangle rect;
	Vector 	  main = makeVector( p1, p2 );
	Vector	  perp = { -main.y, main.x };
	int    	  len  = vectorLen ( perp );

	perp.x *= (WIDTH / 2);
	perp.y *= (WIDTH / 2);
	perp /= len;

	rect.p[0] = perp;

	perp = { main.y, -main.x }
	perp.x *= (WIDTH / 2);
	perp.y *= (WIDTH / 2);
	perp /= len;

	rect.p[1] = perp;

	rect.p[2].x = rect.c2.x + main.x;	
	rect.p[2].y = rect.c2.y + main.y;	

	rect.p[3].x = rect.c1.x + main.x;	
	rect.p[3].y = rect.c1.y + main.y;	

	rect.len = vectorLen( main );
	
	return rect;
}

void rect_init( Rectangle *rect ) {
	rect->x1 = NO_POINT;
	rect->y1 = NO_POINT;

	rect->x2 = NO_POINT;
	rect->y2 = NO_POINT;

	rect->len = 0;
}

int rect_intersect( Rectangle *r1, Rectangle *r2 ) {
	int i;

	// For each edge made by the corners of r1,
	for ( i = 0; i < 2; i ++ ) {
		if ( rect_intersectH( r1, r2, i ) == INTERSECTION ) {
			return INTERSECTION;
		}
	}

	return NO_INTERSECTION;
}

int rect_intersectH( Rectangle *r1, Rectangle *r2, int q ) {
	assert ( q <= 2 );

	Vector edge = r1.p[q+1] - r1.p[q];
	Vector perp = { -edge.y, edge.x };
	int i;
	int side1, side2;
	int r1Side;

	// Determine the side of the other 2 points of R1
	side1 = sign( perp.x * (r1->p3.x - r1->p2.x) + 
				  perp.y * (r1->p3.y - r1->p2.x) );

	side2 = sign( perp.x * (r1->p4.x - r1->p2.x) + 
				  perp.y * (r1->p4.y - r1->p2.x) );

	assert( side1 == side2 );
	
	// Store the side of the first rectangle
	r1Side = side1;

	// Determine the side of each point in the second rect
	for ( i = 0; i < 3; i ++ ) {

		side1 = sign( perp.x * (r2->p1.x - r1->p2.x) + 
					  perp.y * (r2->p1.y - r1->p2.x) );

		// TODO: ZERO return case
		if ( side1 == r1Side ) {
			return INTERSECTION;
		}
	}

	return NO_INTERSECTION;
}
