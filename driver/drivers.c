/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#define DEBUG
#include <ts7200.h>

#include "debug.h"
#include "drivers.h"
#include "error.h"

int timer1Driver (char *retBuf, int buflen) {
	// Get clear location
	int *clearLoc = (int *)(TIMER1_BASE + CLR_OFFSET);

	// Write bit to clear location (handle interrupt)
	*clearLoc = 0x1;

	// Return timer data
	*retBuf = 0;
	return NO_ERROR;
}

int timer2Driver (char *retBuf, int buflen) {
	assert(1==0);
	
	// Get clear location
	int *clearLoc = (int *)(TIMER2_BASE + CLR_OFFSET);

	// Write bit to clear location (handle interrupt)
	*clearLoc = 0x1;

	// Return timer data
	*retBuf = 0;
	return NO_ERROR; 
}
