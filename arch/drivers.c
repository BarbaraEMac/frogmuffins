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
		//debug("1\r\n");
		uart->rsr = 0;			// write to clear memory?
		return SERIAL_OVERRUN;
	}

	// A change in the CTS triggers a Modem status interrupt
	if ( uart->intr & MIS_MASK ) {
		//debug("2\r\n");
		// Reset this interrupt
		uart->intr = 0;

		if ( uart->flag & CTS_MASK ) {
			//debug("3\r\n");
			*data = 0;
			return NO_ERROR;
		} 
		if ( uart->flag & RTS_MASK ) {
			//debug ("RTSSSSS\r\n");
			*data = 0;
			return NO_ERROR;
		}
	// Receive FIFO is empty
	} else if ( uart->intr & RIS_MASK ){
		//debug("4\r\n");
		// reading should reset the interrupt
		// return character if you were told one came in
		//return uart->data;

		data[0] = uart->data;
		// Set the RTS bit
		uart->mctl |= RTS_MASK;

		return NO_ERROR;
	// Transmit FIFO is not full
	} else if ( uart->intr & TIS_MASK ){
		//debug("6\r\n");
		// Implies FIFO is empty
		
		// Turn off this interrupt
		uart->ctlr &= ~(TIEN_MASK);

		*data = 0;
		return NO_ERROR;
	} else {
		debug ("This is the interrupt: %x\r\n", uart->intr);
		error (UNHANDLED_UART_INTR,
			   "UART driver intercepted an unknown uart interrupt!");
	}
	return UNHANDLED_UART_INTR;
}

int uart1Driver (char *data, int len) {
//	debug ("com1driver: data=%s @(%x) len=%d\r\n", data, data, len);
	return uartHandler( UART1, data, len );
}

int uart2Driver (char *data, int len) {
//debug ("com2driver: data=%s @(%x) len=%d\r\n", data, data, len);
	return uartHandler( UART2, data, len );
}

int uart_install ( UART *uart, int speed, int fifo ) {

	// Set the speed
	int err = uart_setSpeed( uart, speed );
	
	// Set up the Request to Send bit
	uart->mctl |= RTS_MASK;

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
