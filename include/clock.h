/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __CLOCK_H__
#define __CLOCK_H__

int* clock_init( int clock_base, int enable, int interrupt, int val );

void clock_stop( int clock_base );

void wait( int ms );

#endif
