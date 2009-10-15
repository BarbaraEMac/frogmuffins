/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
//#define DEBUG

#include <bwio.h>

#include "debug.h"
#include "error.h"
#include "clockserver.h"


void cs_run () {
	debug ("cs_run: The Clock Server is about to start. \r\n");	
	ClockServer cs;
	int 	   senderTid;
	CSRequest  req;
	int 	   ret, len;

	// Initialize the Clock Server
	cs_init (&cs);

	FOREVER {
		// Receive a server request
		debug ("cs: is about to Receive. \r\n");
		len = Receive ( &senderTid, (char *) &req, sizeof(CSRequest) );
		debug ("cs: Received: fromTid=%d name='%s' type=%d  len=%d\r\n", senderTid, req.name, req.type, len);
	
		assert( len == sizeof(CSRequest) );

		switch (req.type) {
			case DELAY:
				
				break;
			case TIME:
				
				break;
			case DELAYUNTIL:
				
				break;
			default:
				ret = CS_INVALID_REQ_TYPE;
				debug (1, "Clockserver request type is not valid.");
				break;
		}

		debug ("cs: about to Reply to %d. \r\n", senderTid);
		Reply ( senderTid, (char *) &ret, sizeof(int) );
		debug ("cs: returned from Replying to %d. \r\n", senderTid);
	}
}

int cs_init (ClockServer *cs) {
	RegisterAs ("ClockServer");
}

