/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __CLOCK_SERVER_H__
#define __CLOCK_SERVER_H__

#define CLOCK_NAME "ClockServer"

#include "globals.h"

typedef struct sleeper Sleeper;

struct sleeper {
	TID tid;
	int endTime;

	Sleeper *next;
	Sleeper *prev;
};

void cs_run ();


// List operators
void list_insert ( Sleeper **head, Sleeper *toAdd );

void list_remove ( Sleeper **head, Sleeper *toAdd );

// Notifier
void notifier_run();

void notifier_init();

// Old clock functions
int* clock_init( int clock_base, int enable, int interrupt, int val );

void clock_stop( int clock_base );

void wait( int ms );

#endif
