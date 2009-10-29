/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#define DEBUG 2

#include <bwio.h>
#include <ts7200.h>

#include "debug.h"
#include "drivers.h"
#include "error.h"
#include "requests.h"
#include "servers.h"

#define DUMMY_TID		0xFF
#define AWAITBUF_LEN	1


//-----------------------------------------------------------------------------
// Private members & Forward Declarations

typedef struct {
	char sendBuffer[NUM_ENTRIES];
	char recvBuffer[NUM_ENTRIES];
	char waiting   [NUM_ENTRIES];	// Array of waiting tids (for a char)

	int sEmpPtr;
	int sFulPtr;
	int rEmpPtr;
	int rFulPtr;
	
	int wEmpPtr;					// Points to next EMPTY slot
	int wFulPtr;					// Points to the first FULL slot
} SerialIOServer;

/**
 * Initialize the Serial IO Server structure.
 */
void ios_init (SerialIOServer *server, UART *uart);

void ios_run (UART *uart);

void ios_attemptTransmit (SerialIOServer *ios, UART *uart);

void ionotifier_run();

void ionotifier_init();

//-----------------------------------------------------------------------------

inline void uart_write( UART *uart, char ch ) {
	uart->data = ch;
	// Turn on the interrupt so that when the FIFO is empty, we will know
	uart->ctlr |= TIEN_MASK;	
}

// Wrapper for UART1 / COM1 / Train Controller!
void ios1_run () {
	debug ("ios1: Running 1\r\n");
	
	int err;
	if ( (err = InstallDriver( INT_UART1, &comOneDriver )) < NO_ERROR ) {
		error(err, "UART1 not installed.");
	}
	debug ("ios1: installed the first driver.\r\n");
	
	ios_run(UART1);
}

// Warpper for UART2 / COM2 / Monitor & Keyboard
void ios2_run () {
	debug ("ios2: Running 2\r\n");
	
	int err;
	if ( InstallDriver( INT_UART2, &comTwoDriver ) < NO_ERROR ) {
		error(err, "UART2 not installed.");
	}
	debug ("ios2: installed the second driver.\r\n");
	
	ios_run(UART2);
}

//TODO: Delete Me if this code works without me!
// Advance the buffer pointer
void advance (int *ptr) {
	*ptr += 1;
	*ptr %= NUM_ENTRIES;
}

// Store the given value in the buffer and increment the pointer
void storeAndAdvancePtr (char *buf, int *ptr, char ch) {
	buf[*ptr] = ch;
	*ptr += 1;
	*ptr %= NUM_ENTRIES;
}

void ios_run (UART *uart) {
	debug ("ios_run: The Serial IO Server is about to start. \r\n");	
	
	int			 senderTid;		// The task id of the message sender
	IORequest	 req;			// A serial io server request message
	int			 len;
	int			 i;
	SerialIOServer ios;

	// Initialize the Serial IO Server
	ios_init (&ios, uart);

	FOREVER {
		// Receive a server request
		debug ("ios: is about to Receive. \r\n");
		len = Receive ( &senderTid, (char *) &req, sizeof(IORequest) );
		debug ("ios: Received: Tid=%d  type=%d len=%d\r\n", 
				senderTid, req.type, len);
	
		assert( len == sizeof(IORequest) );
		assert( senderTid >= 0 );
		
		// Handle the request
		switch (req.type) {
			case NOTIFY_CH:
				debug ("ios: notified CH emp=%d ful=%d\r\n", ios.sEmpPtr, ios.sFulPtr);
				
				// Reply to the Notifier immediately
				Reply (senderTid, (char*) &req.data, sizeof(char));
				
				// If we have a character and someone is blocked, wake them up
				if ( ios.wEmpPtr != ios.wFulPtr ) {
					assert (ios.waiting[ios.wFulPtr] != DUMMY_TID);

					Reply ((int)ios.waiting[ios.wFulPtr], (char *)&req.data, 
							sizeof(char));
					
					storeAndAdvancePtr(ios.waiting, &ios.wFulPtr, DUMMY_TID);
					assert (ios.wFulPtr <= ios.wEmpPtr);
				}
				
				// If we have a character and no one is waiting, store ch
				else {
					assert (ios.recvBuffer[ios.rEmpPtr] == 0);
					
					storeAndAdvancePtr(ios.recvBuffer, &ios.rEmpPtr, req.data[0]);
					assert (ios.rFulPtr <= ios.rEmpPtr);
				}
			
				break;

			case NOTIFY_CTS:
				debug ("ios: notified cts emp=%d ful=%d\r\n", ios.sEmpPtr, ios.sFulPtr);
				
				// Reply to the Notifier immediately
				Reply (senderTid, (char*) &req.data, sizeof(char));
				
				// Try to send the data across the UART!
				if ( ios.sFulPtr != ios.sEmpPtr ) {
					ios_attemptTransmit(&ios, uart);
				}
				break;
			
			case GETC:
				// If we have a character, send it back!
				if (ios.recvBuffer[ios.rFulPtr] != 0) {
					assert (ios.wFulPtr == ios.wEmpPtr);	// Make sure no one else was waiting first ...
					assert (ios.wFulPtr == DUMMY_TID);

					Reply (senderTid, (char *)&ios.recvBuffer[ios.rFulPtr], 
						   sizeof(char));
					
					// Empty the slot and advance the pointer
					storeAndAdvancePtr(ios.recvBuffer, &ios.rFulPtr, 0);
					assert (ios.rFulPtr <= ios.rEmpPtr);
				}
				// Otherwise, store the tid until we get a character
				else {
					assert (ios.waiting[ios.wEmpPtr] == DUMMY_TID);

					storeAndAdvancePtr(ios.waiting, &ios.wEmpPtr, (char)senderTid);
					assert (ios.wFulPtr <= ios.wEmpPtr); 
				}

				break;
			
			case PUTC:
				debug ("ios: putc ch=%c len=%d\r\n", req.data[0], req.len);
			case PUTSTR:
				debug ("ios: putstr str=%s len=%d\r\n", req.data, req.len);
				// Do not let the sender stay blocked for long.
				Reply (senderTid, (char*)&req.data, sizeof(char));

				// Copy the data to our send buffer
				for ( i = 0; i < req.len; i ++ ) {
					assert (ios.sendBuffer[ios.sEmpPtr] == 0);
					
					storeAndAdvancePtr (ios.sendBuffer, &ios.sEmpPtr,
										req.data[i]);
					
					assert (ios.sFulPtr <= ios.sEmpPtr);
				}

				// Try to send some data across the UART
				ios_attemptTransmit (&ios, uart);

				break;
			
			default:
				error (IOS_INVALID_REQ_TYPE, 
					   "Serial IOserver request type is not valid.");
				break;
		}
	}
	
	Exit(); // This will never be called.
}

void ios_init (SerialIOServer *s, UART *uart) {
	int i, err;
	char c;
	
	// Create the notifier
	if ( (err = Create (1, &ionotifier_run)) < NO_ERROR) {
		error (err, "Cannot make IO notifier.\r\n");
	}

	// Empty out the character buffers
	for (i = 0; i < NUM_ENTRIES; i++) {
		s->sendBuffer[i] = 0;
		s->recvBuffer[i] = 0;
		s->waiting[i]    = DUMMY_TID;
	}
	// Init array pointers
	s->sEmpPtr = 0;
	s->sFulPtr = 0;
	s->rEmpPtr = 0;
	s->rFulPtr = 0;
	s->wEmpPtr = 0;					
	s->wFulPtr = 0;				
	
	// Clear out the uart buffer
	while ( uart->flag & RXFF_MASK ) {
		c = (uart->data);
	}

	// TODO: Register with the Name Server
}

void ios_attemptTransmit (SerialIOServer *ios, UART *uart) {
	debug("ios: attempting to transmit ful=%d emp=%d ch=%c\r\n", 
		   ios->sFulPtr, ios->sEmpPtr, ios->sendBuffer[ios->sFulPtr]);
	
	// If the transmitter is busy, wait for interrupt
	if ( uart->flag & TXBUSY_MASK ) {
		debug ("transmit line is busy\r\n");
		uart->ctlr |= TIEN_MASK;	// Turn on the transmit interrupt
	
	// Otherwise, write a single byte
	} else {	
		debug ("writing %c to uart\r\n", ios->sendBuffer[ios->sFulPtr]);

		uart_write ( uart, ios->sendBuffer[ios->sFulPtr] );
	
		// Clear buf & advance pointer
		storeAndAdvancePtr (ios->sendBuffer, &ios->sFulPtr, 0);
		assert (ios->sFulPtr <= ios->sEmpPtr);

		// If we want to send more, enable the interrupt
		if ( ios->sFulPtr != ios->sEmpPtr ) {
			uart->ctlr |= TIEN_MASK;	// Turn on the transmit interrupt
		}
	}
}

void ionotifier_run() {
	int 	  err;
	int 	  serverId = MyParentTid();
	int 	  reply;
	char 	  awaitBuffer;
	IORequest req;

	// Initialize this notifier
	// TODO: get the server id by snychronizing with the parent in inits
	ionotifier_init ();
	
	FOREVER {
		if((err = AwaitEvent(INT_UART1, &awaitBuffer, sizeof(char))) < NO_ERROR){
			// TODO: Handle overrun error
			if ( err == SERIAL_OVERRUN ) {

			}
		}

		// If we do not have a character, we must be clear to send
		if ( awaitBuffer == 0 ) {
			req.type    = NOTIFY_CTS;
			req.data[0] = 0;
			req.len     = 0;
		}
		// Otherwise, give the server the character
		else {
			req.type    = NOTIFY_CH;
			req.data[0] = awaitBuffer;
			req.len     = 1;
		}

		if ( (err = Send( serverId, (char*) &req, sizeof(IORequest),
					     (char *) &reply, sizeof(int) )) < NO_ERROR ) {
			// Handle errors
			
		}


/*
		assert dtr to say you want to send. wait for cts

		// The device driver will clear the interrupt
		//
*/
	}

	Exit(); // This will never be called.
}

void ionotifier_init () {
	debug ("ionotifier_init\r\n");
	
	// TODO: Register
}	
