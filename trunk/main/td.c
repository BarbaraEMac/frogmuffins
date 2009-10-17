/**
 * CS 452: KERNEL
 * becmacdo
 * dgoc
 */
//#define DEBUG
#include <bwio.h>
#include <debug.h>
#include <ts7200.h>

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
	int n = 0;
	BitField field = this->empty[i], mask;
	assert( field != 0 );
	mask = 0xFFFF;
	if ( (field & mask) == 0 ) { n += 16; }
	mask = 0xFF << n;
	if ( (field & mask) == 0 ) { n += 8; }
	mask = 0xF << n;
	if ( (field & mask) == 0 ) { n += 4; }
	mask = 0x3 << n;
	if ( (field & mask) == 0 ) { n += 2; }
	mask = 0x1 << n;
	if ( (field & mask) == 0 ) { n += 1; }

	debug ( "\tmgr_getUnused, empty=%x %x n=%d \r\n", this->empty[1], 
			this->empty[0], n + (i * BITFIELD_WIDTH) );
	return n + (i * BITFIELD_WIDTH);
}


/*
 * PUBLIC FUNCTIONS
 */
void mgr_init ( TDM *this ) {
    int i;
    
	// Each ready priority queue should be empty.
    for ( i = 0; i < NUM_PRIORITY; i ++ ) {
        this->ready[i] = 0;
    }

	// No TDs are blocked at the start
	for ( i = 0; i < NUM_INTERRUPTS; i ++ ) {
		this->intBlocked[i] = 0;
	}

	for ( i = 0; i < NUM_BITFIELD; i++ ) {
		this->empty[i] = 0xFFFFFFFF;
	}

    this->lastId = 0;
	this->highestPriority = NUM_PRIORITY;
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
	
	td->returnValue = 0;
	td->priority = priority;

	// Tasks start as READY
	td->state = READY;

    // Temporary until we insert into TDM
    td->nextTD = 0;
    td->prevTD = 0;

	td->sendQ = 0;

	return td;
}

void td_destroy (TD *td, TDM *mgr) {
	assert( td->state == DEFUNCT );
	int idx = (td->id) % NUM_TDS; 

	// Mark the td as unused
	mgr_setUsed( mgr, idx, 0 );
	/*bwprintf( COM2, "empty=%08x %08x \r\n", mgr->empty[1], mgr->empty[0] );
	bwprintf( COM2, "destroying TD at idx:%d\r\n", idx);
	bwgetc(COM2);*/
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
	
	// If the queue is now empty, change the highest priority pointer.
	if ( this->ready[p] == 0 ) {
		// Find the next highest non-empty slot
		for ( p += 1; p < NUM_PRIORITY && this->ready[p] == 0 ; p++ ) {;}
		debug("		updating priority to %d\r\n", p);	
		this->highestPriority = p;	// This might be off the end by 1
	}

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
