/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#define DEBUG
#include <ts7200.h>

#include "drivers.h"

void timer1Driver (char *retbuf, int buflen) {
	// Get clear location
	int *clearLoc = (int *)(TIMER1_BASE + CLR_OFFSET);

	// Write bit to clear location (handle interrupt)
	*clearLoc = 0x1;

	// Return timer data
	*retBuf = 0;
}

void timer2Driver (char *retbuf, int buflen) {
	// Get clear location
	int *clearLoc = (int *)(TIMER2_BASE + CLR_OFFSET);

	// Write bit to clear location (handle interrupt)
	*clearLoc = 0x1;

	// Return timer data
	*retBuf = 0;
}
