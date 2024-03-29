/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#ifndef __MATH_H__
#define __MATH_H__

#define	NO_POINT		-1
#define NO_INTERSECTION	-1
#define INTERSECTION	 1
#define	NEG				-1
#define	POS				 1
#define	ZERO			 0

typedef struct {
	int x;
	int y;
} Point;

typedef Point Vector;

typedef struct {
	Point p[4];
	int   len;
} Rectangle;

/*
 * Count Trailing Zeroes
 * x - the integer in which to count the leading zeroes
 * RETURNS - the number of consecutive 0s starting with the least significant
 * digit
 */
int ctz( int x );


int log_2( unsigned int x );


/**
 * Absolute value
 * 
 * Returns the absolute value of parameter n ( /n/ ).
 */
int abs( int n );

/**
 * Integer Square Root
 *
 * Copied from http://www.codecodex.com/wiki/Calculate_an_integer_square_root
 */
unsigned long isqrt( unsigned long x );
 
inline int sign (int val);

// ---------------------- Point and Line --------------------------------------
// Find the coord, along the line p1->p2, that is len away from p1.
	   Point 	findPointOnLine ( Point p1, Point p2, int len );
inline int		pointDist( Point p1, Point p2 );
inline Point 	midpoint( Point p1, Point p2 );
inline int	 	slope( Point p1, Point p2 );
inline int 		pointEqual( Point p1, Point p2 );

// --------------------------- Vector -----------------------------------------
inline Vector 	vect_make( Point p1, Point p2 );
inline int  	vect_len( Vector v );
inline Vector 	vect_add( Vector v1, Vector v2 );
inline Vector 	vect_sub( Vector v1, Vector v2 );

// ------------------------ Rectangle -----------------------------------------
	   	Rectangle makeRectangle( Point p1, Point p2 );
inline  void 	rect_init( Rectangle *rect );
		int 	rect_intersect( Rectangle *r1, Rectangle *r2 );
		int 	rect_intersectH( Rectangle *r1, Rectangle *r2, int q );

#endif
