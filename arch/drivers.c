/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#define DEBUG 1
#include <ts7200.h>

#include "debug.h"
#include "drivers.h"
#include "error.h"
#include "requests.h"

int timer1Driver (char *retBuf, int buflen) {
	
	// Get the clock
	Clock *clock = TIMER1;

	// Write bit to clear location (handle interrupt)
	clock->clr = 0x1;

	return NO_ERROR;
}

int timer2Driver (char *retBuf, int buflen) {
	
	// Get the clock
	Clock *clock = TIMER1;

	// Write bit to clear location (handle interrupt)
	clock->clr = 0x1;

	return NO_ERROR; 
}

inline int uartHandler ( UART *uart, char *data, int len ) {
	int intr = uart->intr;

	// Return overrun error if we have already lost data!
	if ( uart->rsr & OE_MASK ) {
		uart->rsr = 0;			// write to clear memory?
		return SERIAL_OVERRUN;
	}

	// A change in the CTS triggers a Modem status interrupt
	if ( intr & MIS_MASK ) {
		// Reset this interrupt
		uart->intr = 0;

		return MODEM_BIT_CHANGE;

	// Receive FIFO is not empty
	} else if ( intr & RIS_MASK ){
		// Return character read that came in (reading resets the interrupt)
		assert( len > 0 );
		data[0] = uart->data;
		
		// UART2 doesn't have the physical lines for RTS/CTS
		if ( uart == UART1 ) { 
			// Set the RTS bit
			uart->mctl |= RTS_MASK;
		}
		
		return RECEIVE_NOT_EMPTY;
	
	// Transmit FIFO is not full
	} else if ( intr & TIS_MASK ){
		// Turn off this interrupt (user code turns it back on when ready)
		uart->ctlr &= ~(TIEN_MASK);

		return TRANSMIT_NOT_FULL;
	} else {
		debug ("This is the interrupt: %x\r\n", intr);
		error (UNHANDLED_UART_INTR,
			   "UART driver intercepted an unknown uart interrupt!");
	}
	return UNHANDLED_UART_INTR;
}

int uart1Driver (char *data, int len) {
	return uartHandler( UART1, data, len );
}

int uart2Driver (char *data, int len) {
	return uartHandler( UART2, data, len );
}

int uart_install ( UART *uart, int speed, int fifo ) {

	// Set the speed
	int err = uart_setSpeed( uart, speed );
	
	// Set up the Request to Send bit
	uart->mctl = RTS_MASK;

	// Always set the HIGH bits after the Mid / Low
	uart_setFifo( uart, fifo );

	switch( (int) uart ) {
		case UART1_BASE:
			err = InstallDriver( INT_UART1, &uart1Driver );
			break;
		case UART2_BASE:
			err = InstallDriver( INT_UART2, &uart2Driver );
			break;
		default:
			return INVALID_UART_ADDR;
	}

	return err;
}
