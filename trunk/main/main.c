/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#define DEBUG 1
#include <bwio.h>
#include <ts7200.h>
#include <math.h>

#include "debug.h"
#include "error.h"
#include "requests.h"
#include "shell.h"
#include "switch.h"
#include "syscalls.h"
#include "task.h"
#include "td.h"

#define WAIT	 for( i=0; i<200000; i++) {}

/**
 * Perform a context switch.
 * Gets the request from a user task.
 * td - The active task.
 * req - The request from a user task.
 */
void getNextRequest ( TD *td, Request *req ) {
	assert ( td != 0 );
	assert ( req != 0 );

	debug( "KERNEL EXIT: (%d) sp=%x spsr=%x pc=%x\r\n", 
			td->id, td->sp, td->spsr, td->sp[PC_OFFSET] );
	
	// Pass the return value
	td->a->retVal = td->returnValue;
	// Context Switch!
	kernelExit (td, req);
	// Reset the return value
	td->returnValue = td->a->retVal;

	debug( "KERNEL ENTRY: (%d) sp=%x spsr=%x pc=%x\r\n", 
			td->id, td->sp, td->spsr, td->sp[PC_OFFSET] );
	debug( "INTERRUPTS: %x\r\n", readMemory(VIC1_BASE + VIC_RAW_INTR) );	
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
	debug( "service: (%d) @%x type:%d args:%x %x %x %x %x \r\n", td->id, td, req->type, 
		req->a->arg[0], req->a->arg[1], req->a->arg[2], 
		req->a->arg[3], req->a->arg[FRAME_SIZE] );
	
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
			td->returnValue = reply (td, mgr, req->a->reply.tid);
			break;
		case AWAITEVENT:
			td->returnValue = awaitEvent (td, mgr, req->a->awaitEvent.eventId );
			break;

		case INSTALLDRIVER:
			td->returnValue = mgr_installDriver(mgr, 
				req->a->installDriver.eventId, req->a->installDriver.driver);
			break;

		case DESTROY:
			td->returnValue = destroy (mgr, td->a->destroy.tid);
			break;

		case EXIT:
			// Set the state to defunct so it never runs again
			td_destroy( td, mgr );
			break;

		case HARDWAREINT:
			debug("HANDLING INTERRUPT type=%d\r\n", req->type);
			handleInterrupt( mgr );
			// fall through
		case PASS:
			// fall through
		default:
			// For now, do nothing
			break;
	}
	if ( td->returnValue < NO_ERROR ) {
		error( td->returnValue, "Kernel request failed.");
		bwprintf( COM2, "service: (%d) @%x type:%d args:%x %x %x %x %x \r\n", td->id, td, 
			req->type, req->a->arg[0], req->a->arg[1], 
			req->a->arg[2], req->a->arg[3], req->a->arg[FRAME_SIZE] );
		bwgetc(COM2);
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

	TD *newActive; 
	// Pop new task off the ready queue
	// If 0 is returned, then we have no more tasks to run.
	newActive = mgr_popReady ( mgr );

	debug ("schedule: scheduled %d @%x\r\n", newActive->id, newActive);
	return newActive;
}

int main( int argc, char* argv[] ) {
	TDM		 mgr;			// The task descriptor manager
	TD		*active;		// A pointer to the actively running task
	Request  nextRequest;	// The next request to service

	// Initialize the printing connection
	uart_setfifo( UART2, OFF );
	bwputstr( COM2, "Initialized serial port connection.\r\n" );
	
	// Set up the Software interrupt for context switches
	writeMemory(0x28, (int) &kernelEnter );
	writeMemory(0x38, (int) &interruptHandler );
	bwprintf( COM2, "Initialized interrupt handlers.\r\n");
	
	// Turn off interrupts 
	intr_allOff();
	bwprintf( COM2, "Initialized interrupt control unit.\r\n");

	// Turn on the cache
	cache_on();
	bwprintf( COM2, "Initialized instruction cache.\r\n");


	// Initialize the priority queues
	mgr_init ( &mgr );

	// Create the first task and set it as the active one
	// Set priority = 0 to ensure that this task completes
	active = td_create ( NUM_PRIORITY-1, &shell_run, -1, &mgr );
	//active = td_create ( 0, &k3_firstUserTask, -1, &mgr );

	if ( active < NO_ERROR ) {
		error ( (int) active, "Initializing the first task");
	}

	FOREVER {	
		active = schedule (active, &mgr);
		
		// Run out of tasks to run
		if ( active == 0 ) {
			break;
		}
		getNextRequest (active, &nextRequest);
		
		service (active, &nextRequest, &mgr);
	}

	// Turn off interrupts 
	bwputstr( COM2, "Turning off interrupts.\r\n");
	intr_allOff();
	
	// Turn on the cache
	bwprintf( COM2, "Disabling instruction cache.\r\n");
	cache_off();
	
	bwputstr( COM2, "Exiting normally.\r\n" );
	
	return 0;
}
