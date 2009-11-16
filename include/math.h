/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#ifndef __MATH_H__
#define __MATH_H__

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

#endif
