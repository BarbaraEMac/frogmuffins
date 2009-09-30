/**
 * CS 452: KERNEL
 * becmacdo
 * dgoc
 */

#include <bwio.h>
#include <ts7200.h>

#include "td.h"
#include "requests.h"
#include "switch.h"
#include "assert.h"

void firstTaskStart () {

    // Create low priority
    bwprintf (COM2, "Created: %d.\n\r", Create (2, &userTaskStart)); 
    bwprintf (COM2, "Created: %d.\n\r", Create (2, &userTaskStart)); 
    // Create high priority
    bwprintf (COM2, "Created: %d.\n\r", Create (0, &userTaskStart)); 
    bwprintf (COM2, "Created: %d.\n\r", Create (0, &userTaskStart)); 

    bwputstr (COM2, "First: exiting.");
 
    Exit();
}


void userTaskStart ( TD *td ) {
    
    bwprintf (COM2, "Tid: %d Parent Tid: %d\n\r", td->id, td->parentId);

    Pass();
    
    bwprintf (COM2, "Tid: %d Parent Tid: %d\n\r", td->id, td->parentId);

    Exit();
}

TD * getUnusedTask ( TDManager *manager ) {
    TD *ret = &manager->tdArray[manager->backPtr];
    manager->backPtr += 1;

    return ret;
}

int getNextId ( TDManager *manager ) {
    int ret = manager->nextId;
    manager->nextId += 1;

    return ret;
}

void initTaskDesc ( TD *td, int priority, void (*s)(), int parentId, TDManager *manager ) {
 	int *pcOnStack;
    
    td->spsr = 0x10;	
	td->sp = 0; //TODO
	pcOnStack = td->sp + PC_OFFSET;

	td->start = s;
	
	td->id = getNextId (manager);
	td->parentId = parentId;
	
	td->returnValue = 0;
	td->priority = priority;
	td->state = READY;

    // Temporary until we insert into PQ
    td->nextPQ = 0;
    td->prevPQ = 0;
}

void insertInReady ( TD *td, TDManager *manager ) {
	int priority = td->priority;

	insertInQueue ( manager->readyQueue[priority], td );
	
	// If this new task has the highest priority,
	// set this pointer. Makes for faster scheduling.
	if ( manager->highestPriority > priority ) {
		manager->highestPriority = priority;
	}

	// Set to ready since it is in the ready queue
	td->state = READY;
}

void insertInBlocked ( TD *td, TDManager *manager ) {
	
	insertInQueue ( manager->blockedQueue, td );
}

void insertInQueue ( TD *head, TD *newTail ) {

	// If the queue was empty, add this as the only item.
	if ( head == 0 ) {
		newTail->prevPQ = newTail;
		newTail->nextPQ = newTail;
		
		head = newTail;
	}
	// Add as a new tail
	else {
		TD *tail = head->prevPQ; // Cannot be 0, could be head

		head->prevPQ = newTail;
		newTail->nextPQ = head;

		tail->nextPQ = newTail;
		newTail->prevPQ = tail;
	}
}

TD * removeFromQueue ( TD *td ) {

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

TD * kernCreateTask ( int priority, void (*start)(), int parentId, TDManager *manager ) {

	// Grab the new task
	TD *newTask = getUnusedTask (manager);

	// Initialize the values
	initTaskDesc ( newTask, priority, start, parentId, manager );

	// Insert into ready queue
    insertInReady (newTask, manager);
	
    return newTask;
}
