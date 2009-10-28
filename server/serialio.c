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

void ios_run () {
	debug ("ios_run: The Serial IO Server is about to start. \r\n");	
	
	int			 senderTid;		// The task id of the message sender
	IORequest	 req;			// A serial io server request message
	int			 len;
	SerialIOServer	server;

	// Initialize the Serial IO Server
	ios_init (&server);

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
			case NOTIFY:
				// If we have a character and someone is blocked, wake them up
				// If we have a character and no one is waiting, store ch
				// If notifier's send buffer needs more, give it entries

			case GETC:
				if ( recvBuffer[rFulPtr] != 0 ) {
					Reply ( &senderTid, (char *)&recvBuffer[rFulPtr], sizeof(char));
					// Advance the pointer
					rFulPtr += 1;
					rFulPtr %= NUM_ENTRIES;

					assert ( rFulPtr <= rEmpPtr );
				}

				break;
			
			case GETSTR:
				// TODO: Probably delete this .....
				break;
			
			case PUTC:
			case PUTSTR:
				// Do not let the sender stay blocked for long.
				Reply ( &senderTid, 0, 0 ); // TODO: fix
				
				// Copy the data to our send buffer
				// TODO: Change this to the notifier's buffer?
				strncpy ( sendBuffer[sEmpPtr], req.data, req.len );
	
				// Advance the buffer pointer
				sEmpPtr += 1;
				sEmpPtr %= NUM_ENTRIES;
				
				assert ( sFulPtr <= sEmpPtr );

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

	
	Create ( 1, &notifier_run );
		


	for (i = 0; i < NUM_ENTRIES; i++) {
		for (j = 0; j < ENTRY_LEN; j++) {
			s->sendBuffer[i][j] = 0;
			s->recvBuffer[i][j] = 0;
		}
	}
	
	// Register with the Name Server
	RegisterAs (SERIAL_IO_NAME);
}

void notifier_run() {
	debug ("notifier_run\r\n");

	int 	  err;
	int 	  serverId = MyParentTid();
	int 	  reply;
	char 	  awaitBuffer[10];
	IOSRequest req;

	char 		chToSend = 0;

	req.type = NOTIFY;
	req.data = 0;
	req.len  = 0;

	// Initialize this notifier
	notifier_init ();
	
	FOREVER {
	//	bwprintf (COM2, "Notifier is awaiting an event\r\n");
		
		if ( (err = AwaitEvent( INT_UART1, awaitBuffer, sizeof(char)*10 )) 
				< NO_ERROR ) {
			// Handle errors
			assert ('a'=='b');
		}
	
		// Check the return buffer?


		if ( (err = Send( serverId, (char*) &req, sizeof(IOSRequest),
					     (char *) &reply, sizeof(int) )) < NO_ERROR ) {
			// Handle errors
			
		}


/*
		only send if you are clear to send
		do not tell server if it is cts
		
		assert dtr to say you want to send. wait for cts




		// The device driver will clear the interrupt
	}

	Exit(); // This will never be called.
}

void notifier_init () {
	debug ("notifier_init\r\n");
	
	// Install the driver
	if ( InstallDriver( INT_UART1, &comOneDriver ) < NO_ERROR ) {
		assert (1==0);
	}

	// TODO: Register
}	
