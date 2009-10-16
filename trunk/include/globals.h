/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#define FOREVER	 for( ; ; )

// TIDs are really ints.
typedef int TID;

// Pointer the beginning of task execution
typedef void (* Task) ();

// Use these to index into the intBlocked array
enum INTERRUPTS {
	COMONE = 0,	
	COMTWO,
	TIMER
} interruptTypes;

#endif
