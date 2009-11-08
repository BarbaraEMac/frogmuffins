
/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#include <bwio.h>
#include <ts7200.h>

#include <debug.h>

void __assert ( int test, char *exp, int line, char *file ) {
	
	if ( !test ) {
		bwprintf (COM2, "\033[31mAssert Failure: %s at line #%d in file %s.\033[37m\n\r", exp, line, file );
		writeMemory(0x28, 0x174c8 );		// swi goes back to redboot

		//asm("mov pc, #0");

	}
}
