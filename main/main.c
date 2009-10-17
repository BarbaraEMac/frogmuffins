/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#define DEBUG
#include <bwio.h>
#include <ts7200.h>

#include "debug.h"
#include "error.h"
#include "requests.h"
#include "switch.h"
#include "syscalls.h"
#include "task.h"
#include "td.h"

#define WAIT	 for( i=0; i<200000; i++) {}

int interruptStatus (int base) {
	int *clear = (int *) (base);
	return *clear;
}
/**
 * Perform a context switch.
 * Gets the request from a user task.
 * td - The active task.
 * req - The request from a user task.
 */
void getNextRequest ( TD *td, Request *req ) {
	assert ( td != 0 );
	assert ( req != 0 );

	debug( "KERNEL EXIT: sp=%x spsr=%x pc=%x\r\n", td->sp, td->spsr,
			td->sp[PC_OFFSET] );
	
	// Context Switch!
	kernelExit (td, req);

	debug( "KERNEL ENTRY: sp=%x spsr=%x pc=%x\r\n", td->sp, td->spsr,
			td->sp[PC_OFFSET] );
	int *raw = (int *) (VIC1_BASE + VIC_RAW_INTR);
	debug( "INTERRUPTS: %x\r\n", *raw );	
	debug( "Request type:%x args:%x %x %x %x %x \r\n", req->type, 
			req->a->send.tid, req->a->send.msg, req->a->send.msglen, 
			req->a->send.reply,req->a->send.rpllen );
}

/**
 * Service a user task's request.
 * td - The active task.
 * req - The request from the user task.
 * mgr - The task descriptor manager with queues.
 */
void service ( TD *td, Request *req, TDM *mgr ) { 
	assert ( td != 0 );
	assert ( req != 0 );
	assert ( mgr != 0 );
	
	TD *child;
	int *i, eventId;
	Driver driver;
	
	// Determine the request type and handle the call
	switch ( req->type ) {
		case CREATE:
			child = td_create(req->a->create.priority, req->a->create.code, td->id, mgr);
			
			// Save an error code if there was one
			td->returnValue = ( (int) child < NO_ERROR ) ? (int) child : child->id;
			break;
		
		case MYTID:
			td->returnValue = td->id;
			break;
		
		case MYPARENTTID:
			td->returnValue = td->parentId;
			break;

		case SEND:
			td->returnValue = send (td, mgr, req->a->send.tid);
			break;
		
		case RECEIVE:
			td->returnValue = receive (td, req->a->receive.tid);
			break;

		case REPLY:
			td->returnValue = reply (td, mgr, req->a->reply.tid, 
						req->a->reply.reply, req->a->reply.rpllen);
			break;
		case AWAITEVENT:
			td->returnValue = awaitEvent (td, mgr, req->a->awaitEvent.eventId );
			break;

		case INSTALLDRIVER:
			td->returnValue = mgr_installDriver(mgr, 
				req->a->installDriver.eventId, req->a->installDriver.driver);
			break;

		case EXIT:
			// Set the state to defunct so it never runs again
			td->state = DEFUNCT;
			td_destroy( td, mgr );
			debug( "Exiting task,\r\n");
			break;

		case HARDWAREINT:
			bwprintf( COM2, "got a HARDWARE INTERRUPT!\r\n" );
			driver =  
			// TODO: handle the interrupt
			i = (int *) (VIC1_BASE + VIC_SOFT_INT_CLR);
			*i = 0xFFFFFFFF;

		case PASS:
		default:
			// For now, do nothing
			break;
	}
}

/**
 * Schedule the next active task to run.
 * oldTask - The previous active task to be added to another queue.
 * mgr - The task descriptor manager with queues.
 */
TD *schedule ( TD *oldTask, TDM *mgr ) {
	assert ( oldTask != 0 );
	assert ( mgr != 0 );
	
	debug ("schedule: active_tid: %d parent: %d p: %d\r\n", oldTask->id,
			oldTask->parentId, oldTask->priority);
	// Push the old active task back on to a queue - ready, blocked, (defunct?)
	mgr_insert ( mgr, oldTask );

	// Pop new task off the ready queue
	// if 0 is returned, then we have no more tasks to run.
	TD *newActive = mgr_popReady ( mgr );

	// Set the state to active since this task is going to run
	if ( newActive != 0 ) {
		newActive->state = ACTIVE;
	}
	
	debug ("schedule: scheduled %d @%x\r\n", newActive->id, newActive);
	return newActive;
}

int main( int argc, char* argv[] ) {
	TDM		 mgr;			// The task descriptor manager
	TD	    *active;		// A pointer to the actively running task
	Request  nextRequest;	// The next request to service

	// Initialize the printing connection
	bwsetfifo( COM2, OFF );
	bwputstr( COM2, "Initialized serial port connection.\r\n" );
	
	// Set up the Software interrupt for context switches
	asm("#; start setting up interrupt handlers");
	int volatile * handler;
	handler = (int *) 0x28;
	*handler = (int) &kernelEnter;
	handler = (int *) 0x38; 
	*handler = (int) &interruptHandler;
	asm("#; done setting up the handlers");
	bwprintf( COM2, "Initialized interrupt handlers at addr %x.\r\n", handler);
	
	// Turn off interrupts 
	handler = (int *) (VIC1_BASE + VIC_INT_ENABLE);
	*handler = 0;

	// Initialize the priority queues
	mgr_init ( &mgr );

	// Create the first task and set it as the active one
    //active = td_create ( 1, &receiverTask, -1, &mgr );
    // Set priority = 0 to ensure that this task completes
	active = td_create ( 0, &k2_firstUserTask, -1, &mgr );

	if ( active < NO_ERROR ) {
		error ( (int) active, "Initializing the first task");
	}

	FOREVER {	
		active = schedule (active, &mgr);
		
		// Run out of tasks to run
		if ( active == 0 ) {
			break;
		}
		if ( active->returnValue < NO_ERROR ) {
			error( active->returnValue, "Kernel request failed.");
		}
		getNextRequest (active, &nextRequest);
		
		service (active, &nextRequest, &mgr);
	}

	bwputstr( COM2, "Exiting normally.\r\n" );
	
	return 0;
}
