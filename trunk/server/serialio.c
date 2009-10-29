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
#define AWAITBUF_LEN	 1


//-----------------------------------------------------------------------------
// Private members & Forward Declarations

typedef struct {
	char sendBuffer[NUM_ENTRIES][ENTRY_LEN];
	char recvBuffer[NUM_ENTRIES];

	int sEmpPtr;
	int sFulPtr;
	int rEmpPtr;
	int rFulPtr;
	
	int waiting[NUM_ENTRIES];		// Array of waiting tids (for a char)
	int wEmpPtr;					// Points to next EMPTY slot
	int wFulPtr;					// Points to the first FULL slot
} SerialIOServer;

/**
 * Initialize the Serial IO Server structure.
 */
void ios_init (SerialIOServer *server);

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
	ios_run(UART1);
}

// Warpper for UART2 / COM2 / Monitor & Keyboard
void ios2_run () {
	ios_run(UART2);
}

// Advance the buffer pointer
void advance (int *ptr) {
	*ptr += 1;
	*ptr %= NUM_ENTRIES;
}

void ios_run (UART *uart) {
	debug ("ios_run: The Serial IO Server is about to start. \r\n");	
	
	int			 senderTid;		// The task id of the message sender
	IORequest	 req;			// A serial io server request message
	int			 len;
	SerialIOServer ios;

	// Initialize the Serial IO Server
	ios_init (&ios);

	FOREVER {
		// Receive a server request
		debug ("ios: is about to Receive. \r\n");
		len = Receive ( &senderTid, (char *) &req, sizeof(IORequest) );
		debug ("ios: Received: Tid=%d  type=%d len=%d\r\n", 
				senderTid, req.type, len);
	
		assert( len == sizeof(IORequest) );
		assert( senderTid > 0 );
		
		// Handle the request
		switch (req.type) {
			case NOTIFY_CH:
				// Reply to the Notifier immediately
				Reply (senderTid, (char*) &req.data, sizeof(char));
				
				// If we have a character and someone is blocked, wake them up
				if ( ios.wEmpPtr != ios.wFulPtr ) {
					assert (ios.waiting[ios.wFulPtr] != DUMMY_TID);

					Reply (ios.waiting[ios.wFulPtr], (char *)&req.data, sizeof(char));
					
					// Reset the array
					ios.waiting[ios.wFulPtr] = DUMMY_TID;
					advance (&ios.wFulPtr);
					assert (ios.wFulPtr <= ios.wEmpPtr);
				}
				
				// If we have a character and no one is waiting, store ch
				else {
					assert ( ios.recvBuffer[ios.rEmpPtr] == 0 );
					
					ios.recvBuffer[ios.rEmpPtr] = req.data[0];
					
					// Advance the pointer
					advance(&ios.rEmpPtr);
					assert (ios.rFulPtr <= ios.rEmpPtr);
				}
			
				break;

			case NOTIFY_CTS:
				// Reply to the Notifier immediately
				Reply (senderTid, (char*) &req.data, sizeof(char));
				
				// If CTS, then put a byte
				uart_write (uart, ios.sendBuffer[ios.sFulPtr]);
				ios.sendBuffer[ios.sFulPtr][0] = 0;

				// Advance the pointer
				advance(&ios.sFulPtr);
				assert (ios.sFulPtr <= ios.sEmpPtr);
				break;
			
			case GETC:
				// If we have a character, send it!
				if (ios.recvBuffer[ios.rFulPtr] != 0) {
					assert (ios.wFulPtr == ios.wEmpPtr);

					Reply (senderTid, (char *)&ios.recvBuffer[ios.rFulPtr], 
						   sizeof(char));
					
					// Advance the pointer
					advance(&ios.rFulPtr);
					assert (ios.rFulPtr <= ios.rEmpPtr);
				}
				// Otherwise, store the tid until we get a character
				else {
					assert (ios.waiting[ios.wEmpPtr] == DUMMY_TID);

					ios.waiting[ios.wEmpPtr] = senderTid;

					advance(&ios.wEmpPtr);
					assert (ios.wFulPtr <= ios.wEmpPtr); 
				}

				break;
			
			case PUTC:
			case PUTSTR:
				// Do not let the sender stay blocked for long.
				Reply (&senderTid, (char*)&req.data, sizeof(char));
				
				assert ( ios.sendBuffer[ios.sEmpPtr] == 0 );

				// Copy the data to our send buffer
				strncpy ( ios.sendBuffer[ios.sEmpPtr], req.data, req.len );
	
				// Advance the buffer pointerp
				advance(&ios.sEmpPtr);
				assert ( ios.sFulPtr <= ios.sEmpPtr );
				
				break;
			
			default:
				error (IOS_INVALID_REQ_TYPE, 
					   "Serial IOserver request type is not valid.");
				break;
		}
	}
	
	Exit(); // This will never be called.
}

void ios_init (SerialIOServer *s) {
	int err;
	int i, j;
	
	Create ( 1, &ionotifier_run );

	for (i = 0; i < NUM_ENTRIES; i++) {
		s->recvBuffer[i] = 0;
		s->waiting[i]    = DUMMY_TID;
		for (j = 0; j < ENTRY_LEN; j++) {
			s->sendBuffer[i][j] = 0;
		}
	}
	
	// Register with the Name Server
	RegisterAs (SERIAL_IO_NAME);
}

void ionotifier_run() {
	debug ("notifier_run\r\n");

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
	debug ("notifier_init\r\n");
	
	// Install the driver
	if ( InstallDriver( INT_UART1, &comOneDriver ) < NO_ERROR ) {
		assert (1==0);
	}

	// TODO: Register
}	
