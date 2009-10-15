/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __CLOCK_SERVER_H__
#define __CLOCK_SERVER_H__

#define CLOCK_NAME "ClockServer"

typedef struct {


} ClockServer;

void cs_run ();

int cs_init (ClockServer *cs);


// Notifier
void notifier_run();


// Old clock functions
int* clock_init( int clock_base, int enable, int val );

void clock_stop( int clock_base );

void wait( int ms );

#endif
