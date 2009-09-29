/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include <bwio.h>
#include <ts7200.h>

#include "switch.h"
#include "requests.h"
#include "td.h"

#define FOREVER	 for( ; ; )
#define WAIT		for( i=0; i<200000; i++) {}
#define NUM_TDs	 64


// TODO: Write an assert library. We will need it.

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
	for( ;; ) {
		bwputstr( COM2, "Task ending.\r\n" );
		asm( "swi #0xDEADBE" );
		bwputstr( COM2, "Task starting\r\n" );
	}
}

TD * initialize ( TDManager *manager ) {
    int i;
    
    for ( i = 0; i < MAX_PRIORITY; i ++ ) {
        manager->pq[i] = 0;
    }

    manager->frontPtr = 0;
    manager->backPtr  = 0;
    manager->nextId   = 0;
    
    return kernCreateTask ( 1, &firstTaskStart, getNextId(manager), manager );
}

void getNextRequest ( TD *td, Request *ret ) {

	kernelExit (td, ret);

}

void service ( TD *td, Request *req, TDManager *manager ) { 
	TD *child;
	
	// Determine the request type and handle the call
	switch ( req->type ) {
		case CREATE:
			child = kernCreateTask(req->arg0, req->arg1, td->id, manager);
			
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
TD * schedule ( TDManager *manager ) {
    TD **pq = manager->pq;
    TD *itr;
    int i;


    // TODO: Starvation? I like starving tasks ...
    for ( i = 0; i < MAX_PRIORITY; i ++ ) {
        itr = pq[i];

        while( itr->priority == i ) {
            if ( itr->state == READY ) {
                return itr;
            }
            itr = itr->nextPQ;
        }

    }
    return 0;
    //    put all defunct at back
    //   if you reach a defunct, start at beginning
    //  if all defunct, return 0;
}


int main( int argc, char* argv[] ) {
	TDManager taskManager;
	TD		*active;
	Request	nextRequest;

	// This is what we will end up with.
	// Look, it's already done.
/*
	active = initialize ( &taskManager );

	FOREVER {
		getNextRequest (active, &nextRequest);
		service (active, &nextRequest, &taskManager);
		active = schedule (&taskManager);
	}
*/
	int data, *junk;
	int i;
	// Set up the Software interrupt for context switches
	int *swi = (int *) 0x28;
	*swi = (int) &kernelEnter;


	// Set up the first task
	TD task1 = { 0x10, 0x21B000-0x24, &test };
	junk = 0x21AFFC;
	*junk = &test;
	Request r1 = {2, 3, 4, 5};
	
	data = (int *) junk;
	
	bwsetfifo( COM2, OFF );
	bwputstr( COM2, "Page table base: " );
	bwputr( COM2, data );
	bwputstr( COM2, "\r\n" );
	bwputr( COM2,	(int) &test);
	for( i=0; i<8; i++ ) {
		bwprintf( COM2, "Going into a context switch sp=%x spsr=%x\r\n", task1.sp, task1.spsr );
		kernelExit(&task1, &r1);
		bwprintf( COM2, "Got back from context switch sp=%x spsr=%x\r\n", task1.sp, task1.spsr );
		bwprintf( COM2, "Returned a0=%x a1=%x a2=%x type=%x\r\n", r1.arg0, r1.arg1, r1.arg2, r1.type );

	}
	bwputstr( COM2, "Exiting normally" );
	return 0;
}
