/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#define NUM_TDS	 64

// TIDs are really ints.
typedef int TID;

// Pointer the beginning of task execution
typedef void (* Task) ();

#endif
