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

// You can Await Event on these
enum INTERRUPTS {
	UART1 = 0,	
	UART2,
	TIMER
} interruptTypes;

#endif
