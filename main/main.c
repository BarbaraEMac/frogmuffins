/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

//#define DEBUG
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

/**
 * Perform a context switch.
 * Gets the request from a user task.
 * td - The active task.
 * req - The request from a user task.
 */
void getNextRequest ( TD *td, Request *req ) {
	assert ( td != 0 );
	assert ( req != 0 );

	debug( "Kernel Exit sp=%x spsr=%x pc=%x\r\n", td->sp, td->spsr,
			td->sp[PC_OFFSET] );
	
	// Context Switch!
	kernelExit (td, req);

	debug( "Kernel Entry sp=%x spsr=%x pc=%x\r\n", td->sp, td->spsr,
			td->sp[PC_OFFSET] );
	
	debug( "Request type:%x args:%x %x %x %x %x \r\n", req->type, 
			req->a->send.tid, req->a->send.msg, req->a->send.msglen, 
			req->a->send.reply,req->a->send.rpllen );
}

/**
 * Service a user task's request.
 * td - The active task.
 * req - The request from the user task.
 * pq - The priority queue of task descriptors.
 */
void service ( TD *td, Request *req, PQ *pq ) { 
	assert ( td != 0 );
	assert ( req != 0 );
	assert ( pq != 0 );
	
	TD *child;
	
	// Determine the request type and handle the call
	switch ( req->type ) {
		case CREATE:
			child = td_create(req->a->create.priority, req->a->create.code, td->id, pq);
			
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
			td->returnValue = send (td, pq, req->a->send.tid);
			break;
		
		case RECEIVE:
			td->returnValue = receive (td, req->a->receive.tid);
			break;

		case REPLY:
			td->returnValue = reply (td, pq, req->a->reply.tid, 
						req->a->reply.reply, req->a->reply.rpllen);
			break;
		
		case EXIT:
			// Set the state to defunct so it never runs again
			td->state = DEFUNCT;
			debug( "Exiting task,\r\n");
			break;

		case PASS:
		default:
			// For now, do nothing
			break;
	}
}

/**
 * Schedule the next active task to run.
 * oldTask - The previous active task to be added to another queue.
 * pq - The priority queues of the task descriptors.
 */
TD *schedule ( TD *oldTask, PQ *pq ) {
	assert ( oldTask != 0 );
	assert ( pq != 0 );
	
	debug ("schedule: active_tid: %d parent: %d p: %d\r\n", oldTask->id,
			oldTask->parentId, oldTask->priority);
	// Push the old active task back on to a queue - ready, blocked, (defunct?)
	pq_insert ( pq, oldTask );

	// Pop new task off the ready queue
	// if 0 is returned, then we have no more tasks to run.
	TD *newActive = pq_popReady ( pq );

	// Set the state to active since this task is going to run
	if ( newActive != 0 ) {
		newActive->state = ACTIVE;
	}
	
	debug ("schedule: scheduled %d (%x)\r\n", newActive->id, newActive);
	return newActive;
}

int main( int argc, char* argv[] ) {
	PQ 		 pq;			// The priority queue manager
	TD	    *active;		// A pointer to the actively running task
	Request  nextRequest;	// The next request to service

	// Initialize the printing connection
	bwsetfifo( COM2, OFF );
	bwputstr( COM2, "Initialized serial port connection.\r\n" );
	
	// Set up the Software interrupt for context switches
	asm("#; start setting up swi handler");
	int volatile * swi = (int *) 0x28;
	*swi = (int) &kernelEnter;
	asm("#; done setting up the handler");
	bwprintf( COM2, "Initialized interrupt handler at addr %x. \r\n", *swi );
	
	// Initialize the priority queues
	pq_init ( &pq );

	// Create the first task and set it as the active one
    //active = td_create ( 1, &receiverTask, -1, &pq );
    // Set priority = 0 to ensure that this task completes
	active = td_create ( 0, &k2_firstUserTask, -1, &pq );

	if ( active < NO_ERROR ) {
		error ( (int) active, "Initializing the first task");
	}

	FOREVER {	
		debug ("	Scheduling a new task.\r\n");
		active = schedule (active, &pq);
		
		// Run out of tasks to run
		if ( active == 0 ) {
			break;
		}

		debug ("	Getting the next request.\r\n");
		getNextRequest (active, &nextRequest);
		debug ("	Got a new request successfully.\r\n");
		
		service (active, &nextRequest, &pq);
		debug ("	Done servicing the request.\r\n");
	}

	bwputstr( COM2, "Exiting normally.\r\n" );
	
	return 0;
}
