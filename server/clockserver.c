/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
//#define DEBUG

#include <bwio.h>
#include <ts7200.h>

#include "clockserver.h"
#include "debug.h"
#include "error.h"
#include "requests.h"

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

			case NOTIFY:


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
	RegisterAs (CLOCK_NAME);
	
	// Spawn a notifying helper task
	Create( 1, &notifier_run );
}


void notifier_run() {

}

int* clock_init( int clock_base, int enable, int val ) {
    int *clock_ldr = (int *)( clock_base + LDR_OFFSET );
    int *clock_ctl = (int *)( clock_base + CRTL_OFFSET );
    // set the loader value, first disabling the timer
    *clock_ctl = 0;
    *clock_ldr = val;
    // set the enable bit
    if( enable ) { *clock_ctl = ENABLE_MASK; }
    // return a pointer to the clock
    return (int *)( clock_base + VAL_OFFSET );
}

void clock_stop( int clock_base ) {
    clock_init( clock_base, 0, 0);
}

void wait( int ms ) {
    int * clock = clock_init( TIMER1_BASE, 1, ms*2 );   // turn on the clock
    while( *clock > 0 ) {}                              // wait for the clock to finish 
    clock_stop( TIMER1_BASE );                          // turn off the clock
}
