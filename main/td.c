/**
 * CS 452: KERNEL
 * becmacdo
 * dgoc
 */
//#define DEBUG
#include <bwio.h>
#include <ts7200.h>

#include "td.h"
#include "requests.h"
#include "switch.h"
#include "assert.h"



void pq_init ( PQ *this ) {
    int i;
    
    for ( i = 0; i < NUM_PRIORITY; i ++ ) {
        this->ready[i] = 0;
    }

    this->frontPtr = 0;
    this->backPtr  = 0;
    this->nextId   = 0;
	this->highestPriority = NUM_PRIORITY;
	this->blocked = 0;
}

TD * td_init ( int priority, Task start, int parentId, PQ *pq ) {
    assert( pq->backPtr < 64 );
	TD *td = &pq->tdArray[pq->backPtr++];

	// TODO THIS WILL RUN OFF THE END
	td->id = pq->nextId++;

    td->spsr = 0x10;	
	td->sp = (int *) 0x220000 + (0x40000*pq->backPtr) - 0x40; //TODO
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

void pq_insert ( PQ *this, TD *td ) {
	// When we first create a task, it is ready.
	// We should insert it into the ready queue
	assert ( td != 0 );

	int p = td->priority;
	debug("pq_insert td: %x priority: %d\r\n", td, p);
	
	switch ( td->state ) {
		case ACTIVE:
			td->state = READY;

		case READY:
			
			queue_push ( &this->ready[p], td );
			
			// If this new task has the highest priority,
			// set this pointer. Makes for faster scheduling.
			if ( p < this->highestPriority ) {
				this->highestPriority = p;
				debug ("	  HIGHEST PRIORITY: %d\r\n", p);
			}

			// Set to ready since it is in the ready queue
			td->state = READY;
			
			break;

		case BLOCKED:
			queue_push ( &this->blocked, td );
			break;
		
		case DEFUNCT:
		default:
			debug ("We are NOT adding %x at p: %d to a queue.\r\n", td, p);
			break;
	}
	debug("pq_insert %x DONE\r\n", td);
}

TD *pq_popReady ( PQ *this ) {
	int p = this->highestPriority;
	debug("pq_popReady PQ:%x priority:%d\r\n", this, p);

	if ( p == NUM_PRIORITY ) {
		return 0;
	}

	// this needs to have something
	assert (this->ready[p] != 0);

	TD *top = queue_pop ( &this->ready[p] );
	if ( this->ready[p] == 0 ) {
		debug ("POPPPPPP: readyQueue head: %x p:%d\r\n", this->ready[p], p);
		
		// Find the next highest non-empty slot
		for ( p += 1; p < NUM_PRIORITY && this->ready[p] == 0 ; p++ ) {;}
		
		this->highestPriority = p;	// This might be off the end by 1
		
		debug ("		HIGHEST PRIORITY: %d\r\n", p);
	}

	debug("pq_popReady %x DONE\r\n");
	return top;
}

void queue_push ( Queue *q, TD *newTail ) {
	debug ( "queue_push head=%x, newTail=%x \r\n", *q, newTail );
	
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
		TD *tail = head->prevPQ; // Cannot be 0, could be head
		// Update inserted's elements pointers
		newTail->nextPQ = head;
		newTail->prevPQ = tail;
		// Put the element in the queue
		head->prevPQ = newTail;
		tail->nextPQ = newTail;
	}
}

TD *queue_pop (Queue *head) {
	debug ( "queue_pop head=%x \r\n", *head );

	TD *top = *head;

	assert (top->prevPQ != 0);
	assert (top->nextPQ != 0);

	TD *prev = top->prevPQ; // Can be top itself
	TD *next = top->nextPQ; // Can be top itself

	prev->nextPQ = next;	// Reset the links
	next->prevPQ = prev;
	
	top->nextPQ = 0;		// Ensure this task points to nothing
	top->prevPQ = 0;

	// if top == next, we've popped the last one
	debug ("pop: top: %x prev: %x next: %x\r\n", top, prev, next);
	*head = (top == next) ? 0 : next;

	return top; 
}

TD * td_create (int priority, Task start, int parentId, PQ *pq) {
	
	debug ( "td_create: priority=%d, parent=%d this %x\r\n", priority, parentId, pq);
	// Grab the new task
	TD *newTask = td_init ( priority, start, parentId, pq );
	debug ( "	init task %d\r\n", newTask );

	// Insert into ready queue
    pq_insert (pq, newTask);
	debug ( "	inserted in ready queue\r\n" );
	
    return newTask;
}
