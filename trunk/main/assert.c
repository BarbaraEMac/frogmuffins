
/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include "assert.h"

#include <bwio.h>
#include <ts7200.h>

void __assert ( int test, char *exp, char *line, char *file ) {
	
	if ( !test ) {
		bwprintf (COM2, "Assert Failure: %s at line #%s in file %s.\n\r", exp, line, file );
		// TODO: Stack trace
	}
}





