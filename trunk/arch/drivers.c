/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

//#define DEBUG
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
	
	// Get clear location
	int *clearLoc = (int *)(TIMER2_BASE + CLR_OFFSET);

	// Write bit to clear location (handle interrupt)
	*clearLoc = 0x1;

	// Return timer data
	*retBuf = 0;
	return NO_ERROR; 
}

inline int comHandler ( UART *uart, char *data, int len ) {
	// TODO: Return overrun errors - we already lost the data!
	

	if ( uart->intr & MIS_MASK ) {

		if ( uart->flag & CTS_MASK ) {


		} 
		//else if ( uart->flag & ) {


//		}
	
	} else if ( uart->intr & RIS_MASK ){
		// return character if you were told one came in
		return uart->data;
	
	} else if ( uart->intr & TIS_MASK ){
	
		int i;
		for ( i = 0; i < len; i ++ ) {
			// TODO: Wait 52 instructions
			
			uart->data = data[i];
		}
		return NO_ERROR;
	}
	// Never gets here ...
}

int comOneDriver (char *data, int len) {
	UART *uart = (UART*) (UART1_BASE);
	return comHandler( uart, data, len );
}

int comTwoDriver (char *data, int len) {
	UART *uart = (UART*) (UART2_BASE);
	return comHandler( uart, data, len );
}
