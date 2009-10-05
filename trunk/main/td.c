/**
 * CS 452: KERNEL
 * becmacdo
 * dgoc
 */
//#define DEBUG
#include <bwio.h>
#include <debug.h>
#include <ts7200.h>

#include "td.h"
#include "requests.h"
#include "switch.h"

void pq_init ( PQ *this ) {
    int i;
    
	// Each ready priority queue should be empty.
    for ( i = 0; i < NUM_PRIORITY; i ++ ) {
        this->ready[i] = 0;
    }

    this->frontPtr = 0;
    this->backPtr  = 0;
    this->nextId   = 0;
	this->highestPriority = NUM_PRIORITY;
	this->blocked = 0;
}

TD * td_create (int priority, Task start, TID parentId, PQ *pq) {
	debug ( "td_create: priority=%d, parent=%d pq=%x\r\n",
			priority, parentId, pq);
	
	if ( priority < 0 || priority > NUM_PRIORITY ) {
		return -1;
	}
	
	assert ( pq != 0 );

	// Grab the new task
	TD *newTask = td_init ( priority, start, parentId, pq );
	
	if ( newTask == 0 ) {
		return -2;
	}

	// TODO: Check for valid start function. Return -1

	// Insert into ready queue
    pq_insert (pq, newTask);
	
    return newTask;
}

TD * td_init ( int priority, Task start, TID parentId, PQ *pq ) {
	debug ( "td_init: priority=%d, parent=%d pq=%x\r\n",
			priority, parentId, pq);
    assert ( pq->backPtr < 64 );
	assert ( priority >= 0 );
	assert ( priority < NUM_PRIORITY );
	assert ( pq != 0 );
	
	// Grab an unused TD from the array.
	// TODO: Replace this once we have dynamic memory allocation.
	TD *td = &pq->tdArray[pq->backPtr++];

	// TODO: THIS WILL RUN OFF THE END
	td->id = pq->nextId++;

    td->spsr = 0x10;	
	td->sp = (int *) 0x220000 + (0x40000*pq->backPtr) - 0x40; //TODO
	td->sp[PC_OFFSET] = (int) start;
	
	td->parentId = parentId;
	
	td->returnValue = 0;
	td->priority = priority;

	// Tasks start as READY
	td->state = READY;

    // Temporary until we insert into PQ
	// TODO: this is wasting instructions
    td->nextPQ = 0;
    td->prevPQ = 0;

	return td;
}

void pq_insert ( PQ *this, TD *td ) {
	debug ("pq_insert this/pq=%x td=%x priority=%d.\r\n",
			this, td, td->priority);
	assert ( this != 0 );
	assert ( td != 0 );

	int p = td->priority;
	switch ( td->state ) {
		
		case ACTIVE:
			// Active tasks should be made ready and put on a ready queue.
			td->state = READY;

		case READY:
			// Add the TD to the ready queue
			queue_push ( &this->ready[p], td );
			
			// If this new task has the highest priority,
			// set this pointer. Makes for faster scheduling.
			if ( p < this->highestPriority ) {
				this->highestPriority = p;
			}

			// Set to ready since it is in the ready queue
			td->state = READY;
			
			break;

		case BLOCKED:
			// Add the TD to the blocked queue.
			queue_push ( &this->blocked, td );
			break;
		
		case DEFUNCT:
		default:
			// Do nothing for defunct tasks for now ...
			debug ("We are NOT adding %x at p: %d to a queue.\r\n", td, p);
			break;
	}
}

TD *pq_popReady ( PQ *this ) {
	debug("pq_popReady this/pq=%x priority=%d\r\n",
		   this, this->highestPriority);
	assert ( this != 0 );

	int p = this->highestPriority;

	// If p == NUM_PRIORITY, there are no TDs in any ready queue.
	if ( p == NUM_PRIORITY ) {
		return 0;
	}

	// This ready queue MUST have something.
	assert (this->ready[p] != 0);

	// Pop off the top of the queue.
	TD *top = queue_pop ( &this->ready[p] );
	
	// If the queue is now empty, change the highest priority pointer.
	if ( this->ready[p] == 0 ) {
		// Find the next highest non-empty slot
		for ( p += 1; p < NUM_PRIORITY && this->ready[p] == 0 ; p++ ) {;}
		
		this->highestPriority = p;	// This might be off the end by 1
	}

	// Return the top of the queue
	return top;
}

void queue_push ( Queue *q, TD *newTail ) {
	debug ( "queue_push head=%x, newTail=%x \r\n", *q, newTail );
	assert ( q != 0 );
	assert ( newTail != 0 );
	
	TD *head = *q;

	// If the queue was empty, add this as the only item.
	if ( head  == 0 ) {
		newTail->prevPQ = newTail;
		newTail->nextPQ = newTail;

		// Add this to the queue
		*q = newTail;
	}
	// Push as a new tail
	else {
		TD *tail = head->prevPQ; // Cannot be 0; Could be head
		assert ( tail != 0 );
		assert ( tail->nextPQ != 0 );
		
		// Update inserted's elements pointers
		newTail->nextPQ = head;
		newTail->prevPQ = tail;
		
		// Put the element in the queue
		head->prevPQ = newTail;
		tail->nextPQ = newTail;
	}

	assert ( head != 0 );
}

TD *queue_pop (Queue *head) {
	debug ( "queue_pop head=%x \r\n", *head );
	assert ( head != 0 );

	TD *top = *head;
	assert ( top != 0 );
	assert ( top->prevPQ != 0 );
	assert ( top->nextPQ != 0 );

	TD *prev = top->prevPQ; // Cannot be 0; Can be top itself
	TD *next = top->nextPQ; // Cannot be 0; Can be top itself

	prev->nextPQ = next;	// Reset the links
	next->prevPQ = prev;
	
	top->nextPQ = 0;		// Ensure this task points to nothing
	top->prevPQ = 0;

	// If top == next, we've popped the last one on this queue.
	debug ("	queue_pop: top: %x prev: %x next: %x\r\n", top, prev, next);
	*head = (top == next) ? 0 : next;

	// Return the old top of the queue
	return top; 
}
