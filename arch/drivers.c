/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#define DEBUG 1
#include <bwio.h>
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

inline int uartHandler ( UART *uart, char *data, int len ) {
	// Return overrun error if we have already lost data!
	if ( uart->rsr & OE_MASK ) {
		debug("1\r\n");
		uart->rsr = 0;			// write to clear memory?
		return SERIAL_OVERRUN;
	}

	// A change in the CTS triggers a Modem status interrupt
	if ( uart->intr & MIS_MASK ) {
		debug("2\r\n");
		// Reset this interrupt
		uart->intr = 0;

		if ( uart->flag & CTS_MASK ) {
			debug("3\r\n");
			*data = 0;
			return NO_ERROR;
		} 
	
	} else if ( uart->intr & RIS_MASK ){
		debug("4\r\n");
		// reading should reset the interrupt
		// return character if you were told one came in
		//return uart->data;

		data[0] = uart->data;
		// Set the RTS bit
		uart->mctl |= RTS_MASK;

		return NO_ERROR;
	
	} else if ( uart->intr & RTIS_MASK ){
		debug("5\r\n");

		// TODO: We might want to do something else in this case

		data[0] = uart->data;
		return NO_ERROR;
	
	} else if ( uart->intr & TIS_MASK ){
		debug("6\r\n");
		// Implies FIFO is empty
		
		// Turn off this interrupt
		uart->ctlr &= ~(TIEN_MASK);

		*data = 0;
		return NO_ERROR;
	}
	debug("THIS IS BAD. THIS SHOULD NEVER PRINT\r\n");
	// Never gets here ...
}

int uart1Driver (char *data, int len) {
	debug ("com1driver: data=%s @(%x) len=%d\r\n", data, data, len);
	return uartHandler( UART1, data, len );
}

int uart2Driver (char *data, int len) {
	debug ("com2driver: data=%s @(%x) len=%d\r\n", data, data, len);
	return uartHandler( UART2, data, len );
}
