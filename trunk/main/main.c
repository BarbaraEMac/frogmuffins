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

void getNextRequest ( TD *td, Request *ret ) {

	debug( "Kernel Exit sp=%x spsr=%x pc=%x\r\n", td->sp, td->spsr, td->sp[PC_OFFSET] );

	kernelExit (td, ret);

	debug( "Kernel Entry sp=%x spsr=%x pc=%x\r\n", td->sp, td->spsr, td->sp[PC_OFFSET] );
	debug( "Request type:%x args:%x %x %x %x %x \r\n", ret->type, ret->args[0], ret->args[1], ret->args[2], ret->args[3], ret->args[22] );
}

void service ( TD *td, Request *req, TDManager *manager ) { 
	TD *child;
	
	// Determine the request type and handle the call
	switch ( req->type ) {
		case CREATE:
			child = kernCreateTask(req->args[0], (Task) req->args[1], td->id, manager);
			
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

// Schedule the next task to run?
// Probably do some fun scheduling algorotihm here
TD * schedule ( TD *oldTask, TDManager *manager ) {
    TD **readyQ = manager->readyQ;
	
	// Handle the previous task
	switch (oldTask->state) {
		case ACTIVE:
			insertInReady (oldTask, manager);
			break;
		case BLOCKED:
			insertInBlocked (oldTask, manager);
			break;
		case DEFUNCT:
		default:
			break;
	}
	
	int highest = manager->highestPriority;
	
	// Pop new task off the ready queue
	Queue ret = readyQ[highest];
	Queue newHead = queuePop ( ret );
	
	if ( newHead == ret ) {
		newHead = 0;	// TODO: fix this hack
		
		// Reset the highest priority pointer
		int i = highest + 1;

		// Find the next highest non-empty slot
		for ( ; i < NUM_PRIORITY; i ++ ) {
			if ( readyQ[i] != 0 ) {
				manager->highestPriority = i;
			}
		}
		
		// If we have no tasks left, set to an error code.
		if ( manager->highestPriority == highest ) {
			manager->highestPriority = NUM_PRIORITY;
		}
	}

	// Reassign the head - either to the new task OR 0
	manager->readyQ[highest] = newHead;
	
	// Set the state to active since this task is going to run
	ret->state = ACTIVE;
	return ret;
}

int main( int argc, char* argv[] ) {
	bwsetfifo( COM2, OFF );
	// Set up the Software interrupt for context switches
	int *swi = (int *) 0x28;
	*swi = (int) &kernelEnter;
	bwputstr( COM2, "Initialized interrupt handler.\r\n" );
	
	TDManager taskManager;
	TD		*active;
	Request	nextRequest;

	// This is what we will end up with.
	// Look, it's already done.

	debug( "Location of firsttask: %x\r\n", &firstTaskStart );
	managerInit ( &taskManager );
    active = kernCreateTask ( 1, &firstTaskStart, -1, &taskManager );
/*	FOREVER {
		getNextRequest (active, &nextRequest);
		service (active, &nextRequest, &taskManager);
		active = schedule (active, &taskManager);
	}
//*/

	int i;


	// Set up the first task
//	TD task1 = { 0x10, (int *) 0x21B000-0x38 };
//	task1.sp[PC_OFFSET] = (int) &firstTaskStart;
	
	for( i=0; i<8; i++ ) {
		debug( "about to get next\r\n" );
		getNextRequest (active, &nextRequest);
		debug( "done getting next\r\n" );
		service (active, &nextRequest, &taskManager);
		active = schedule (active, &taskManager);
	}
	bwputstr( COM2, "Exiting normally" );
	//*/
	return 0;
}
