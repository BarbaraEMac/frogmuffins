/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#define DEBUG 1 
#include <bwio.h>
#include <ts7200.h>

#include "debug.h"
#include "error.h"
#include "requests.h"
#include "shell.h"
#include "switch.h"
#include "syscalls.h"
#include "td.h"


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

	assert( td->sp > td->sb - STACK_SIZE );

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
	
	int ret = NO_ERROR;
	TD *child;
	debug( "service: (%d) @%x type:%d args:%x %x %x %x %x \r\n", td->id, td, req->type, 
		req->a->arg[0], req->a->arg[1], req->a->arg[2], 
		req->a->arg[3], req->a->arg[FRAME_SIZE] );
	
	// Determine the request type and handle the call
	switch ( req->type ) {
		case CREATE:
			child = td_create(req->a->create.priority, req->a->create.code, td->id, mgr);
			
			// Save an error code if there was one
			ret = td->returnValue = ( (int) child < NO_ERROR ) ? (int) child : child->id;
			break;
		
		case MYTID:
			ret = td->returnValue = td->id;
			break;
		
		case MYPARENTTID:
			ret = td->returnValue = td->parentId;
			break;

		case SEND:
			ret = td->returnValue = send (td, mgr, req->a->send.tid);
			break;
		
		case RECEIVE:
			ret = td->returnValue = receive (td, req->a->receive.tid);
			break;

		case REPLY:
			ret = td->returnValue = reply (td, mgr, req->a->reply.tid);
			break;
		case AWAITEVENT:
			ret = td->returnValue = awaitEvent (td, mgr, req->a->awaitEvent.eventId );
			break;

		case INSTALLDRIVER:
			ret = td->returnValue = mgr_installDriver(mgr, 
				req->a->installDriver.eventId, req->a->installDriver.driver);
			break;

		case DESTROY:
			ret = td->returnValue = destroy (mgr, td->a->destroy.tid);
			break;

		case EXIT:
			// Set the state to defunct so it never runs again
			td_destroy( td, mgr );
			break;

		case HARDWAREINT:
			handleInterrupt( mgr );
			// fall through
		case PASS:
			// fall through
		default:
			// For now, do nothing
			break;
	}
	if ( ret < NO_ERROR ) {
		error( td->returnValue, "Kernel request failed.");
		bwprintf( COM2, "service: (%d) @%x type:%d args:%x %x %x %x %x \r\n",
			td->id, td, req->type, req->a->arg[0], req->a->arg[1], 
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

	// turn off FIFO so we can print the following messages:
	uart_setFifo( UART2, OFF );

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

	// Create the shell task and set it as the active one
	active = td_create ( NUM_PRIORITY-2, &bootstrap, -1, &mgr );
	if_error ( (int) active, "Error initializing the shell.");

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
