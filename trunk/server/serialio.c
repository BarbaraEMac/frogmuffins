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

#define DUMMY_TID		-1
#define DUMMY_CH		0xFF
#define AWAITBUF_LEN	1


//-----------------------------------------------------------------------------
// Private members & Forward Declarations

typedef struct {
	char sendBuffer[NUM_ENTRIES];
	char recvBuffer[NUM_ENTRIES];
	int waiting    [NUM_ENTRIES];	// Array of waiting tids (for a char)

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

	// TODO: we might not want to turn the interrupt on here
	// we ONLY want it on iff there is another character to write
	// Turn on the interrupt so that when the FIFO is empty, we will know
	uart->ctlr |= TIEN_MASK;	
}

// Wrapper for UART1 / COM1 / Train Controller!
void ios1_run () {
	debug ("ios1: Running 1\r\n");
	
	int err = InstallDriver( INT_UART1, &comOneDriver );
	if_error(err, "UART1 driver not installed.");
	
	debug ("ios1: installed the first driver.\r\n");

	err = uart_setspeed( UART1, 2400 );
	if_error(err, "UART1 speed not set.");

	// TODO remove the next 3 lines
	UART *uart = UART1;
	assert( uart->lcrm == 0 );
	assert( uart->lcrl == 0xbf );

	ios_run( UART1 );
}

// Warpper for UART2 / COM2 / Monitor & Keyboard
void ios2_run () {
	debug ("ios2: Running 2\r\n");
	
	int err = InstallDriver( INT_UART2, &comTwoDriver );
	if_error(err, "UART2 driver not installed.");
	
	debug ("ios2: installed the second driver.\r\n");
	
	err = uart_setspeed( UART2, 115200 );
	if_error(err, "UART2 speed not set.");

	ios_run( UART2 );
}

//TODO: Delete Me if this code works without me!
// Advance the buffer pointer
void advance (int *ptr) {
	*ptr += 1;
	*ptr %= NUM_ENTRIES;
}

// Store the given value in the buffer and increment the pointer
void storeCh (char *buf, int *ptr, char ch) {
	buf[*ptr] = ch;

	debug ("STORE ch=%c (%d)\r\n", ch, ch);
	advance(ptr);
}

void storeInt (int *buf, int *ptr, int item) {
	buf[*ptr] = item;

	debug ("STORE int=%d\r\n", item);
	advance (ptr);
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
					
					debug ("ios: waking up %d to give ch=%c\r\n",
							ios.waiting[ios.wFulPtr], req.data[0]);
					Reply ((int)ios.waiting[ios.wFulPtr], (char *)&req.data, 
							sizeof(char));
					
					storeInt(ios.waiting, &ios.wFulPtr, DUMMY_TID);
					assert (ios.wFulPtr <= ios.wEmpPtr);
				}
				
				// If we have a character and no one is waiting, store ch
				else {
					debug("ios: notified is storing ch=%c\r\n", req.data[0]);
					assert (ios.recvBuffer[ios.rEmpPtr] == DUMMY_CH);
					assert (ios.waiting[ios.wFulPtr] == DUMMY_TID);
					
					storeCh(ios.recvBuffer, &ios.rEmpPtr, req.data[0]);
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
				//if (ios.recvBuffer[ios.rFulPtr] != DUMMY_CH) {
				if (ios.rFulPtr != ios.rEmpPtr ) {
					debug ("ios: getc has a char=%c ful=%d emp=%d wait:ful=%d emp=%d\r\n",
							ios.recvBuffer[ios.rFulPtr], ios.rFulPtr, ios.rEmpPtr,
							ios.wFulPtr, ios.wEmpPtr);
					assert (ios.recvBuffer[ios.rFulPtr] != DUMMY_CH);
					// Make sure no one else was waiting first ...
					assert (ios.wFulPtr == ios.wEmpPtr);	
					assert (ios.waiting[ios.wFulPtr] == DUMMY_TID);

					Reply (senderTid, (char *)&ios.recvBuffer[ios.rFulPtr], 
						   sizeof(char));
					
					// Empty the slot and advance the pointer
					storeCh(ios.recvBuffer, &ios.rFulPtr, DUMMY_CH);
					assert (ios.rFulPtr <= ios.rEmpPtr);
				}
				// Otherwise, store the tid until we get a character
				else {
					debug ("ios: getc NO char ful=%d emp=%d wait:ful=%d emp=%d\r\n",
							ios.rFulPtr, ios.rEmpPtr, ios.wFulPtr, ios.wEmpPtr);
					assert (ios.waiting[ios.wEmpPtr] == DUMMY_TID);

					storeInt(ios.waiting, &ios.wEmpPtr, (char)senderTid);
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
					assert (ios.sendBuffer[ios.sEmpPtr] == DUMMY_CH);
					
					bwputc(COM2, req.data[i]);
					storeCh (ios.sendBuffer, &ios.sEmpPtr,
										req.data[i]);
					
					assert (ios.sFulPtr <= ios.sEmpPtr);
				}

				bwputstr(COM2, "\r\n");

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
	
	// TODO: Send/Receive to synchronize with the notifier

	// Empty out the character buffers
	for (i = 0; i < NUM_ENTRIES; i++) {
		s->sendBuffer[i] = DUMMY_CH;
		s->recvBuffer[i] = DUMMY_CH;
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
	while ( uart->flag & RXFF_MASK ) { c = (uart->data); }
	
	// Enable the UART
	uart->ctlr |= UARTEN_MASK;
	
	// Turn the interrupt ON
	uart->ctlr |= RIEN_MASK;
	// TODO: Getting a C might work without this on .. I didn't try
	uart->ctlr |= RTIEN_MASK;

	// Register with the name server
	// TODO: FIX THIS NAME TO REFLECT COM1 & COM2
	RegisterAs (SERIALIO_NAME);
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
		storeCh (ios->sendBuffer, &ios->sFulPtr, 0);
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
		debug ("NOTIFIER WOKE UP\r\n");
		// If we do not have a character, we must be clear to send
		if ( awaitBuffer == 0 ) {
			req.type    = NOTIFY_CTS;
			req.data[0] = 0;
			req.len     = 0;
		}
		// Otherwise, give the server the character
		else {
			debug ("HAS A CHARACTER\r\n");
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
}	
