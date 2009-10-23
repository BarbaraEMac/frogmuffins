/**
 * IO 452: Kernel
 * becmacdo
 * dgoc
 */
//#define DEBUG

#include <bwio.h>
#include <ts7200.h>

#include "debug.h"
#include "drivers.h"
#include "error.h"
#include "requests.h"
#include "serialio.h"

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
			case GETC:

				break;
			case PUTC:

				break;

			case GETSTR:

				break;

			case PUTSTR:


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

	// Register with the Name Server
	RegisterAs (SERIAL_IO_NAME);

	for (i = 0; i < NUM_ENTRIES; i++) {
		for (j = 0; j < ENTRY_LEN; j++) {
			s->sendBuffer[i][j] = 0;
			s->recvBuffer[i][j] = 0;
		}
	}
}
