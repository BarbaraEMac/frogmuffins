/**
 * CS 452: KERNEL
 * becmacdo
 * dgoc
 */
//#define DEBUG
#include <debug.h>
#include <ts7200.h>
#include <math.h>

#include "error.h"
#include "requests.h"
#include "switch.h"
#include "td.h"

#define BITFIELD_WIDTH sizeof(BitField)*8

/*
 * HELPER FUNCTIONS
 */
void mgr_setUsed ( TDM *this, unsigned int idx, int used ) {
	debug ( "\tmgr_setUsed this=%x used=%d, idx=%d\r\n", this, used, idx );
	assert ( idx < NUM_TDS );
	unsigned int i = idx / (BITFIELD_WIDTH);// the index of the bitfield
	unsigned int s = idx % (BITFIELD_WIDTH);// the position in the bitfield
	if ( used ) {
		this->empty[i] &= ~(1 << s);
	} else {
		this->empty[i] |= (1 << s);
	}
}

int mgr_isEmpty ( TDM *this, unsigned int idx ) {
	debug ( "\tmgr_isEmpty this=%x, idx=%d\r\n", this, idx );
	assert ( idx < NUM_TDS );
	unsigned int i = idx / (BITFIELD_WIDTH);// the index of the bitfield
	unsigned int s = idx % (BITFIELD_WIDTH);// the position in the bitfield
	
	int ans =  ( this->empty[i] & (1 << s) ); 
	debug ( "\tmgr_isEmpty return=%d\r\n", ans );
	return ans;
}

int mgr_getUnused ( const TDM *this ) {
	debug ( "\tmgr_getUnused this=%x\r\n", this );
	int i;
	// find the first non-full bitfield
	for ( i = 0; i < NUM_BITFIELD && this->empty[i]==0; i ++ ) {}

	if ( i >= NUM_BITFIELD ) {	// if everything's full we're screwed
		return NO_TDS_LEFT;
	}
	int n = ctz(this->empty[i]) + (i * BITFIELD_WIDTH);

	debug ( "\tmgr_getUnused, empty=%x %x n=%d \r\n", this->empty[1], 
			this->empty[0], n);
	return n;
}

void mgr_updatePriority( TDM*this, int p ) {

	// If the queue is now empty, change the highest priority pointer.
	if ( this->ready[p] == 0 ) {
		// Find the next highest non-empty slot
		for ( p += 1; p < NUM_PRIORITY && this->ready[p] == 0 ; p++ ) {;}
		debug("		updating priority to %d\r\n", p);	
		this->highestPriority = p;	// This might be off the end by 1
	}
}

/*
 * PUBLIC FUNCTIONS
 */
void mgr_init ( TDM *this ) {
    int i;
    
	// Each ready priority queue should be empty.
    for ( i = 0; i < NUM_PRIORITY; i++ ) {
        this->ready[i] = 0;
    }

	// No TDs are blocked at the start
	for ( i = 0; i < NUM_INTERRUPTS; i++ ) {
		this->intBlocked[i] = 0;
	}

	// No drivers are installed at the start
	for ( i = 0; i < NUM_INTERRUPTS; i++ ) {
		this->intDriver[i] = 0;
	}

	for ( i = 0; i < NUM_BITFIELD; i++ ) {
		this->empty[i] = 0xFFFFFFFF;
	}

    this->lastId = 0;
	this->highestPriority = NUM_PRIORITY;
}

int mgr_installDriver( TDM *this, int eventId, Driver driver ) {

	if( eventId >= NUM_INTERRUPTS || eventId < 0) {
		return INVALID_EVENTID;
	}
	// Make sure we haven't already installed a driver for this yet.
	assert ( this->intDriver[eventId] == 0 );

	this->intDriver[eventId] = driver;
	return NO_ERROR;	// success
}

TD * td_create (int priority, Task start, TID parentId, TDM *mgr) {
	debug ( "td_create: priority=%d, parent=%d mgr=%x\r\n",
			priority, parentId, mgr);
	
	if ( priority < 0 || priority > NUM_PRIORITY ) {
		return (TD *) INVALID_PRIORITY;
	}
	
	assert ( mgr != 0 );

	// Grab the new task
	TD *newTask = td_init ( priority, start, parentId, mgr );
	
	if ( (int) newTask < NO_ERROR ) {
		return newTask;
	}

	// Insert into ready queue
    mgr_insert (mgr, newTask);
	
    return newTask;
}

TD * td_init ( int priority, Task start, TID parentId, TDM *mgr ) {
	debug ( "td_init: priority=%d, parent=%d mgr=%x\r\n",
			priority, parentId, mgr);
	assert ( priority >= 0 );
	assert ( priority < NUM_PRIORITY );
	assert ( mgr != 0 );
	
	// Grab an unused TD from the array.
	int idx = mgr_getUnused ( mgr );
	if ( idx < NO_ERROR ) {
		return (TD *) idx;
	}
	TD *td = &mgr->tdArray[idx];
	
	// Mark the slot as used
	mgr_setUsed( mgr, idx, 1 );

	int diff = idx - (mgr->lastId % NUM_TDS); 
	mgr->lastId += diff;
	td->id = mgr->lastId;

    td->spsr = DEFAULT_PSR;	
	td->sb = (int *) STACK_BASE + (STACK_SIZE * idx); 
	td->sp = td->sb - 16; 	// leave space for the 'stored registers'
	asm( "#; look after this");
	td->sp[PC_OFFSET] = (int) start;
	asm( "#; but before this");
	
	td->parentId = parentId;
	td->priority = priority;
	td->returnValue = 0;

	// Tasks start as READY
	td->state = READY;

    // Temporary until we insert into TDM
    td->nextTD = 0;
    td->prevTD = 0;

	td->sendQ = 0;

	return td;
}

void td_destroy (TD *td, TDM *mgr) {
	debug( "td_destroy TD at idx:%d\r\n", idx);
	int idx = (td->id) % NUM_TDS; 

	// Mark the td as unused
	mgr_setUsed( mgr, idx, 0 );

	int p = td->priority, eventId;
	TD *waitOn;
	switch ( td->state ) {

		case READY:
			// Remove the TD from the ready queue
			queue_remove ( &mgr->ready[p], td );
			
			// Update the priority if needed
			mgr_updatePriority( mgr, p );
			break;

		case RCV_BLKD:
			waitOn = mgr_fetchById( mgr, td->a->send.tid );
			assert( (int) waitOn >= NO_ERROR );
			// Remove the TD from the send queue
			queue_remove ( &waitOn->sendQ, td );
			break;
		case AWAITING_EVT:
			eventId = td->a->awaitEvent.eventId;
			// Remove the TD from the interrupt queue
			queue_remove ( &mgr->intBlocked[eventId], td );
			if( mgr->intBlocked[eventId] == 0 ) {
				// Turn off interrupts as there is no-one to handle them
				intr_set ( eventId, OFF );
			}
			break;

		case ACTIVE:
			// Fall through
		case RPLY_BLKD:
			// Fall through
		case SEND_BLKD:
			// Fall through
		case DEFUNCT:
			// Nothing to do
			break;
		default:
			error(0, "Don't know what to do with this task state." );
			break;
	}

	td->state = DEFUNCT;
}

void mgr_insert ( TDM *this, TD *td ) {
	debug ("mgr_insert this=%x td=%x priority=%d state=%d.\r\n",
			this, td, td->priority, td->state);
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

		case SEND_BLKD:
			debug ("%x is SEND_BLKD \r\n", td);
			break;
		case RCV_BLKD:
			debug ("%x is RCV_BLKD \r\n", td);
			break;
		case RPLY_BLKD:
			debug ("%x is RPLY_BLKD \r\n", td);
			break;
		case AWAITING_EVT:
			debug ("%x is AWAITING_EVT \r\n", td);
			break;

		case DEFUNCT:
		default:
			// Do nothing for defunct tasks for now ...
			debug ("We are NOT adding %x at p: %d to a queue.\r\n", td, p);
			break;
	}
}

TD *mgr_popReady ( TDM *this ) {
	debug("mgr_popReady this=%x priority=%d\r\n", this, this->highestPriority);
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

	// Update the priority if needed
	mgr_updatePriority( this, p );

	// Set the task to active since it's going to run
	top->state = ACTIVE;

	// Return the top of the queue
	return top;
}

TD *mgr_fetchById ( TDM *this, TID tid ) {
	debug("mgr_fetchById this=%x tid=%d\r\n", this, tid);
	// Verify td > 0
	if ( tid < 0 ) {
		return (TD *) NEG_TID;
	}

	int idx = tid % NUM_TDS;
	
	// Check if TD is in use
	if ( mgr_isEmpty( this, idx) ) {
		return (TD *) INVALID_TID;
	}

	// Fetch the appropriate TD
	TD *ret = &this->tdArray[idx];
	
	// Verify tid points to a valid td
	if ( ret->id > tid ) {
		return (TD *) OLD_TID;
	}

	// Verify td is not defunct
	if ( ret->state == DEFUNCT ) {
		return (TD *) DEFUNCT_TID;
	}

	debug("mgr_fetchById TD=%x \r\n", ret);
	return ret;
}

void queue_push ( Queue *q, TD *newTail ) {
	debug ( "queue_push head=%x, newTail=%x \r\n", *q, newTail );
	assert ( q != 0 );
	assert ( newTail != 0 );
	
	TD *head = *q;

	// If the queue was empty, add this as the only item.
	if ( head  == 0 ) {
		newTail->prevTD = newTail;
		newTail->nextTD = newTail;

		// Add this to the queue
		*q = newTail;
	}
	// Push as a new tail
	else {
		TD *tail = head->prevTD; // Cannot be 0; Could be head
		assert ( tail != 0 );
		assert ( tail->nextTD != 0 );
		
		// Update inserted's elements pointers
		newTail->nextTD = head;
		newTail->prevTD = tail;
		
		// Put the element in the queue
		head->prevTD = newTail;
		tail->nextTD = newTail;
	}

	assert ( *q != 0 );
}

TD *queue_pop (Queue *head) {
	debug ( "queue_pop head=%x \r\n", *head );
	assert ( head != 0 );

	TD *top = *head;
	assert ( top != 0 );
	assert ( top->prevTD != 0 );
	assert ( top->nextTD != 0 );

	TD *prev = top->prevTD; // Cannot be 0; Can be top itself
	TD *next = top->nextTD; // Cannot be 0; Can be top itself

	prev->nextTD = next;	// Reset the links
	next->prevTD = prev;
	
	top->nextTD = 0;		// Ensure this task points to nothing
	top->prevTD = 0;

	// If top == next, we've popped the last one on this queue.
	debug ("	queue_pop: top: %x prev: %x next: %x\r\n", top, prev, next);
	*head = (top == next) ? 0 : next;

	// Return the old top of the queue
	return top; 
}
void queue_remove ( Queue *q, TD *toRm ) {
	debug ( "queue_push head=%x, toRm=%x \r\n", *q, toRm );
	assert ( q != 0 );
	assert ( toRm != 0 );
	assert ( toRm->nextTD != 0 && toRm->prevTD != 0 );

	TD *prev = toRm->prevTD; // Cannot be 0; Can be itself
	TD *next = toRm->nextTD; // Cannot be 0; Can be itself

	// Remove the element from the queue
	prev->nextTD = next;
	next->prevTD = prev;

	// Update the removed element's pointers
	toRm->prevTD = 0;
	toRm->nextTD = 0;

	// If this the first item, queue will have to be updated
	if ( *q  == toRm ) {
		// Update the queue
		*q = prev->nextTD;
	}

}
