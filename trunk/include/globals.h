/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#ifndef __GLOBALS_H__
#define __GLOBALS_H__

#define FOREVER	 for( ; ; )

#define foreach( item, array ) \
      for(item = array; item < (array + sizeof(array)/sizeof*(item)); item++)


#define array_size( array ) (sizeof(array)/sizeof(*array))

// TIDs are really ints.
typedef int TID;

// Pointer the beginning of task execution
typedef void (* Task) ();
#endif
