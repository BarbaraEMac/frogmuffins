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

void test( ) {
	int i =0, id= 0;
	for( ;; ) {
		bwprintf( COM2, "Task ending. i=%d\r\n", i );
		id = Test5(5, 4, 3, 2, 0xFEEDDEAD);
		i++;
		bwprintf( COM2, "Task starting i=%d, id=%x\r\n", i, id );
	}
}

TD * initialize ( TDManager *manager ) {
    int i;
    
    for ( i = 0; i < MAX_PRIORITY; i ++ ) {
        manager->readyQueue[i] = 0;
    }

    manager->frontPtr = 0;
    manager->backPtr  = 0;
    manager->nextId   = 0;
	manager->highestPriority = 0;
	manager->blockedQueue = 0;
    
    return kernCreateTask ( 1, &firstTaskStart, -1, manager );
}

void getNextRequest ( TD *td, Request *ret ) {
	bwprintf (COM2, "%x\n\r", td->sp);

	bwprintf( COM2, "Kernel Exit sp=%x spsr=%x pc=%x\r\n", td->sp, td->spsr, td->sp[PC_OFFSET] );

	kernelExit (td, ret);

	bwprintf( COM2, "Kernel Entry sp=%x spsr=%x pc=%x\r\n", td->sp, td->spsr, td->sp[PC_OFFSET] );
	bwprintf( COM2, "Returned a0=%x a1=%x a2=%x a3=%x a4=%x type=%x\r\n", ret->args[0], ret->args[1], ret->args[2], ret->args[3], ret->args[22], ret->type );
}

void service ( TD *td, Request *req, TDManager *manager ) { 
	TD *child;
	
	// Determine the request type and handle the call
	switch ( req->type ) {
		case CREATE:
			child = kernCreateTask(req->args[0], (Task) req->args[1], td->id, manager);
			
			td->returnValue = child->id;
			//break;
		
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
    TD **readyQueue = manager->readyQueue;
	
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
	TD *ret = readyQueue[highest];
	TD *newHead = removeFromQueue ( ret );
	
	if ( newHead == ret ) {
		newHead = 0;	// TODO: fix this hack
		
		// Reset the highest priority pointer
		int i = highest + 1;

		// Find the next highest non-empty slot
		for ( ; i <= MAX_PRIORITY; i ++ ) {
			if ( readyQueue[i] != 0 ) {
				manager->highestPriority = i;
			}
		}
		
		// If we have no tasks left, set to an error code.
		if ( manager->highestPriority == highest ) {
			manager->highestPriority = MAX_PRIORITY + 1;
		}
	}

	// Reassign the head - either to the new task OR 0
	manager->readyQueue[highest] = newHead;
	
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

	bwprintf( COM2, "Location of firsttask: %x\r\n", &firstTaskStart );
	active = initialize ( &taskManager );
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
		bwputstr (COM2, "about to get next\n\r");
		getNextRequest (active, &nextRequest);
		bwputstr (COM2, "done getting next");
		service (active, &nextRequest, &taskManager);
		//active = schedule (active, &taskManager);
	}
	bwputstr( COM2, "Exiting normally" );
	//*/
	return 0;
}
