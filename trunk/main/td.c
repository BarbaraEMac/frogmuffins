/**
 * CS 452: KERNEL
 * becmacdo
 * dgoc
 */
#define DEBUG
#include <bwio.h>
#include <ts7200.h>

#include "td.h"
#include "requests.h"
#include "switch.h"
#include "assert.h"

void firstTaskStart () {

    // Create low priority
    bwprintf (COM2, "Created: %d.\r\n", Create (2, &userTaskStart)); 
    bwprintf (COM2, "Created: %d.\r\n", Create (2, &userTaskStart)); 
    // Create high priority
    bwprintf (COM2, "Created: %d.\r\n", Create (0, &userTaskStart)); 
    bwprintf (COM2, "Created: %d.\r\n", Create (0, &userTaskStart)); 

    bwputstr (COM2, "First: exiting.");
 
    Exit();
}


void userTaskStart ( ) {
    
    bwprintf (COM2, "Tid: %d Parent Tid: %d\r\n", MyTid(), MyParentTid() );

    Pass();
    
    bwprintf (COM2, "Tid: %d Parent Tid: %d\r\n", MyTid(), MyParentTid() );

    Exit();
}

void managerInit ( TDManager *manager ) {
    int i;
    
    for ( i = 0; i < NUM_PRIORITY; i ++ ) {
        manager->readyQ[i] = 0;
    }

    manager->frontPtr = 0;
    manager->backPtr  = 0;
    manager->nextId   = 0;
	manager->highestPriority = 0;
	manager->blockedQ = 0;
    
}

TD * initNewTaskDesc ( int priority, Task start, int parentId, TDManager *manager ) {
    assert( manager->backPtr < 64 );
	TD *td = &manager->tdArray[manager->backPtr++];

	// TODO THIS WILL RUN OFF THE END
	td->id = manager->nextId++;

    td->spsr = 0x10;	
	td->sp = (int *) 0x220000 + (0x40000*manager->backPtr) - 0x40; //TODO
	td->sp[PC_OFFSET] = (int) start;
	
	td->parentId = parentId;
	
	td->returnValue = 0;
	td->priority = priority;
	td->state = READY;

    // Temporary until we insert into PQ
	// TODO: this is wasting instructions
    td->nextPQ = 0;
    td->prevPQ = 0;

	return td;
}

void insertInReady ( TD *td, TDManager *manager ) {
	int p = td->priority;

	debug ( "about to insert\r\n");
	manager->readyQ[p] = queueAdd ( manager->readyQ[p], td );
	
	// If this new task has the highest priority,
	// set this pointer. Makes for faster scheduling.
	if ( manager->highestPriority > p ) {
		manager->highestPriority = p;
	}

	// Set to ready since it is in the ready queue
	td->state = READY;
}

void insertInBlocked ( TD *td, TDManager *manager ) {
	
	manager->blockedQ =	queueAdd ( manager->blockedQ, td );
}

Queue queueAdd ( Queue head, TD *newTail ) {
	debug ( "queueAdd head=%x, newTail=%x \r\n", head, newTail );
	
	// If the queue was empty, add this as the only item.
	if ( head == 0 ) {
		newTail->prevPQ = newTail;
		newTail->nextPQ = newTail;
	}
	// Add as a new tail
	else {
		TD *tail = head->prevPQ; // Cannot be 0, could be head
		// Update inserted's elements pointers
		newTail->nextPQ = head;
		newTail->prevPQ = tail;
		// Put the element in the queue
		head->prevPQ = newTail;
		tail->nextPQ = newTail;
	}
	return newTail;
	debug ( "new head=%x\r\n", head );
}

Queue queuePop ( TD *td ) {
	debug ( "queuePop td=%x \r\n", td );

	assert (td->prevPQ != 0);
	assert (td->nextPQ != 0);

	TD *prev = td->prevPQ; // Can be td itself
	TD *next = td->nextPQ; // Can be td itself

	prev->nextPQ = next;	// Reset the links
	next->prevPQ = prev;
	
	td->nextPQ = 0;			// Ensure this task points to nothing
	td->prevPQ = 0;

	return next; // could be td - which is confusing
}

TD * kernCreateTask ( int priority, Task start, int parentId, TDManager *manager ) {
	
	debug ( "kernCreateTask: priority=%d, parent=%d manager %x\r\n", priority, parentId, manager);
	// Grab the new task
	TD *newTask = initNewTaskDesc ( priority, start, parentId, manager );
	debug ( "	init task %d\r\n", newTask );

	// Insert into ready queue
    insertInReady (newTask, manager);
	debug ( "	inserted in ready queue\r\n" );
	
    return newTask;
}
