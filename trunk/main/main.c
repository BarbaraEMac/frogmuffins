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

#define FOREVER     for( ; ; )
#define WAIT        for( i=0; i<200000; i++) {}
#define NUM_TDs     64


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

void initialize ( TD *tds ) {
    // Init things to zero?


}

void getNextRequest ( TD *td, Request *ret ) {

    kerExit (td, ret);

}

void service ( TD *td, Request *nxt, TaskManager *pq ) { 
    
    // Determine the request type and handle the call
    switch (nxt->type) {
        case CREATE:
            createTask(nxt->arg0, nxt->arg1, td->id, pq);
            

            break;
        case MYTID:
            // Return td->id

            break;
        case MYPARENTTID:
            // return td->parentId;


            break;

        case PASS:


            break;
        case EXIT:
            // Set the state to defunct so it never runs again
            td->state = DEFUNCT;
            break;
    }
}

// Schedule the next task to run?
// Probably do some fun scheduling algorotihm here
TD * schedule ( TD *taskPs ) {
    

}

void createTask ( int priority, void (*start)(), int parentId, TaskManager *pq ) {

    // Grab the new task
    TD *newTask = pq->tdArray[pq->backPtr];
 
    // Initialize the values
    // TODO: Put into a nice function
    newTask->spsr = // TODO: ????
    newTask->sp = // TODO: ????
    newTask->start = start;
    
    newTask->id = manager->nextId;
    newTask->parentId = parentId;
    
    newTask->returnValue = //TODO: ???
    newTask->priority = priority;
    newTask->state = READY;

    // Insert into PQ
    TD *nextTask = manager->pq[priority];
    TD *prevTasl = nextTask->prevPQ;

    prevTask

    manager->backPtr += 1;   // Advance pointer
    manager->nextId += 1;       // Advance next valid task id
}
typedef struct {
    
    TD tdArray [64];
    int frontPtr;
    int backPtr;

    TD taskPriorities [4]; // We have 5 priorities

    int nextId;

} TaskManager;


int main( int argc, char* argv[] ) {
    TaskManager taskPQ;

    TD active;
    Request nextRequest;

    // This is what we will end up with.
    // Look, it's already done.
/*
    active = initialize ( taskDescriptors, taskPriorities );

    FOREVER {
        getNextRequest (active, &nextRequest);
        service (active, nextRequest);
        active = schedule (taskPriorities);
    }
*/

    int data, *junk;
	int i;
	// Set up the Software interrupt for context switches
	int *swi = (int *) 0x28;
	*swi = (int) &kerEnt;


	// Set up the first task
	TD task1 = { 0x00000010, 0x21B000-0x24, &test };
	junk = 0x21AFFC;
	*junk = &test;
	Request r1 = {2, 3, 4, 5};
	
    data = (int *) junk;
	
    bwsetfifo( COM2, OFF );
    bwputstr( COM2, "Page table base: " );
    bwputr( COM2, data );
    bwputstr( COM2, "\r\n" );
    bwputr( COM2,  (int) &test);
	for( i=0; i<8; i++ ) {
		bwprintf( COM2, "Going into a context switch sp=%x spsr=%x\r\n", task1.sp, task1.spsr );
		kerExit(&task1, &r1);
		bwprintf( COM2, "Got back from context switch sp=%x spsr=%x\r\n", task1.sp, task1.spsr );
		bwprintf( COM2, "Returned a0=%x a1=%x a2=%x type=%x\r\n", r1.arg0, r1.arg1, r1.arg2, r1.type );

	}
    bwputstr( COM2, "Exiting normally" );
    return 0;
}
