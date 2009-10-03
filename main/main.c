/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#define DEBUG
#include <bwio.h>
#include <ts7200.h>

#include "switch.h"
#include "requests.h"
#include "td.h"
#include "assert.h"

#define FOREVER	 for( ; ; )
#define WAIT	 for( i=0; i<200000; i++) {}
#define NUM_TDs	 64

/*
// the following funciton was copied and modified from wikipedia
int rev_log2(unsigned char x) {
	int r = 0;
	while ((x >> r) != 0) { r++; }
	return 9-r; // returns -1 for x==0, floor(log2(x)) otherwise
}

// zero-fill a char array
void charset( char*str, int len, char ch=0 ) {
	while( (--len) >= 0 ) str[len] = ch;
}*/


/**
 * After the first user task, all following tasks run this function.
 */
void userTaskStart ( ) {
    
	// Print your information
    bwprintf (COM2, "Tid: %d Parent Tid: %d\r\n", MyTid(), MyParentTid());

	// Let someone else run
    Pass();
    
	// Print your information again
    bwprintf (COM2, "Tid: %d Parent Tid: %d\r\n", MyTid(), MyParentTid());

    Exit();
}

/*
 * The first user task runs this function.
 */
void firstTaskStart () {

    // Create low priority
    bwprintf (COM2, "Created: %d.\r\n", Create (2, &userTaskStart)); 
    bwprintf (COM2, "Created: %d.\r\n", Create (2, &userTaskStart)); 
    
	// Create high priority
    bwprintf (COM2, "Created: %d.\r\n", Create (0, &userTaskStart)); 
    bwprintf (COM2, "Created: %d.\r\n", Create (0, &userTaskStart)); 

	// Exit
    bwputstr (COM2, "First: exiting.\r\n");
    Exit();
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

	debug( "Kernel Exit sp=%x spsr=%x pc=%x\r\n", td->sp, td->spsr,
			td->sp[PC_OFFSET] );
	
	// Context Switch!
	kernelExit (td, req);

	debug( "Kernel Entry sp=%x spsr=%x pc=%x\r\n", td->sp, td->spsr,
			td->sp[PC_OFFSET] );
	
	debug( "Request type:%x args:%x %x %x %x %x \r\n", req->type, 
			req->args[0], req->args[1], req->args[2], req->args[3],
			req->args[22] );
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
			child = td_create(req->args[0], (Task) req->args[1], td->id, pq);
			
			td->returnValue = child->id;
			break;
		
		case MYTID:
			td->returnValue = td->id;
			break;
		
		case MYPARENTTID:
			td->returnValue = td->parentId;
			break;
		
		case EXIT:
			// Set the state to defunct so it never runs again
			td->state = DEFUNCT;
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
	assert ( td != 0 );
	assert ( pq != 0 );
	
	debug ("schedule tid: %d parent: %d p: %d\r\n", oldTask->id,
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
	
	return newActive;
}

int main( int argc, char* argv[] ) {
	PQ 		 pq;			// The priority queue manager
	TD	    *active;		// A pointer to the actively running task
	Request  nextRequest;	// The next request to service

	// Initialize the printing connection
	bwputstr( COM2, "Initializing serial port connection.\r\n" );
	bwsetfifo( COM2, OFF );
	
	// Set up the Software interrupt for context switches
	bwputstr( COM2, "Initializing interrupt handler.\r\n" );
	int *swi = (int *) 0x28;
	*swi = (int) &kernelEnter;
	
	// Initialize the priority queues
	pq_init ( &pq );

	// Create the first task and set it as the active one
    active = td_create ( 1, &firstTaskStart, -1, &pq );

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
