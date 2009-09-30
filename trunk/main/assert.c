
/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include "assert.h"

#include <bwio.h>
#include <ts7200.h>

void __assert ( int exp, char *mess ) {
	
	if ( !exp ) {
		bwprintf (COM2, "Assert Failure: %s.\n\r", mess );
		// TODO: Stack trace
	}
}





