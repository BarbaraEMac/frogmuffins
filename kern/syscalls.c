/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#define DEBUG 1
#include <ts7200.h>
#include <string.h>
#include <math.h>

#include "debug.h"
#include "error.h"
#include "syscalls.h"
#include "td.h"

int isValidMem ( const char *addr, const size_t size, const TD *td ) {
	if ( size == 0 ) return NO_ERROR;		// if no message to pass we don't care
	if ( addr == 0 ) return NULL_ADDR;		// a very special case here

	if ( (addr + size >= (char *) td->sb) || ((int*)addr < td->sb-STACK_SIZE) ) {
		return OUT_OF_BOUNDS;
	}

	return NO_ERROR;
}

int send (TD *sender, TDM *mgr, TID tid) {
	debug ("send: from=%x, to=%x (%d)\r\n", sender, mgr_fetchById(mgr, tid), tid);
	// Check all arguments
	assert ( sender != 0 );
	assert ( sender->state == ACTIVE );
	assert ( mgr != 0 );
	int ret = NO_ERROR;

  	// Do not send a message to yourself.
	if ( sender->id == tid )
		return INVALID_TID;

	// Verify the pointers point to valid memory addresses
	ret = isValidMem( sender->a->send.msg, sender->a->send.msglen, sender );
	if (ret != NO_ERROR ) return ret; 

	ret = isValidMem( sender->a->send.reply, sender->a->send.rpllen, sender );
	if (ret != NO_ERROR ) return ret; // TODO Add a mask here so that we can tell is apart

	
	TD *receiver = mgr_fetchById (mgr, tid);
	// mgr_fetchById error checks the tid
	if ( (int) receiver < NO_ERROR ) 
		return (int) receiver;
	
	// Set state to receive blocked
	sender->state = RCV_BLKD;

	// Is the receiver send blocked?
	if ( receiver->state == SEND_BLKD ) {
		// Verify sender and receiver states
		assert ( sender->state == RCV_BLKD );
		assert ( receiver->state == SEND_BLKD );
		
		// Pass the message
		ret = passMessage ( sender, receiver, SEND_2_RCV );
		assert (sender->state == RPLY_BLKD);

		// Pass the sender tid to the receiver
		*receiver->a->receive.tid = sender->id;

		// Unblock the receiver
		mgr_insert(mgr, receiver);
	} else {
		// Put yourself on the other task's send queue.
		queue_push ( &receiver->sendQ, sender );
	}

	return ret;
}

int receive (TD *receiver, TID *tid) {
	debug ("rcv : rcvr=%x (%d)\r\n", receiver, receiver->id);

	// Check all arguments
	assert ( receiver != 0 );
	int ret = NO_ERROR;

	// Verify stack addresses are valid
	ret = isValidMem(receiver->a->receive.msg, receiver->a->receive.msglen, receiver );
	if (ret != NO_ERROR ) return ret; 

	// Change the state to SEND_BLKD
	receiver->state = SEND_BLKD;

	// If someone is on the send queue, complete the transaction.
	if ( receiver->sendQ != 0 ) {
		TD *sender = queue_pop ( &receiver->sendQ );

		// Verify sender and receiver states
		assert ( sender->state == RCV_BLKD );
		assert ( receiver->state == SEND_BLKD );
		
		ret = passMessage ( sender, receiver, SEND_2_RCV );
		assert (receiver->state == READY);

		// Set the tid of the sender
		*tid = sender->id;
	}

	return ret;
}

int reply (TD *from, TDM *mgr, TID tid) {
	debug ("rply: from @%x (%d) to (%d) \r\n", from, from->id, tid);
	assert ( from != 0 );
	assert ( mgr != 0 );
	assert ( from->id != tid ); // Do not reply to yourself.
	int ret = NO_ERROR;

	// FROM is really the receiver OR replier if a worker task was spawned
	// TO is really the sender that we are acking

	// Update the TD states
	TD *to = mgr_fetchById ( mgr, tid );
	// mgr_fetchById error checks the tid
	if ( (int) to < NO_ERROR ) {
		return (int) to;
	}
	
	// If the sender is not reply blocked, we should error.
	assert (to->state == RPLY_BLKD);
	if ( to->state != RPLY_BLKD ) {
		return SNDR_NOT_RPLY_BLKD;
	}
	
	// Check the msg pointer
	ret = isValidMem( from->a->reply.reply, from->a->reply.rpllen, from );
	if (ret != NO_ERROR ) return ret; 

	// Copy the data over
	ret = passMessage ( from, to, REPLY_2_SEND );
	// Even if PassMessage failed keep going
	assert (to->state == READY);
	assert (from->state == READY);

	// Make the sender (to) task ready by putting it on a ready queue
	// NOTE: 'from' will be put on queue by send() if it was blocked
	mgr_insert ( mgr, to );

	return ret;
}

/*
 * The arguments passed in should already be checked
 * this method will return BUFFER_FULL errors if the source buffer
 * lenght is longer than the destination
 * Updates the states of the tasks
 */
int passMessage ( TD *from, TD *to, MsgType type ) {
	debug("passMessage: from @%x to @%x type=%d\r\n", from, to, type);
	assert ( to != from );
	// Get the message buffers and lengths
	char  *source = from->a->send.msg;
	size_t sourceLen = from->a->send.msglen;
	
	char  *dest = to->a->receive.msg;
	size_t destLen = to->a->receive.msglen;


	// If we are doing a reply, fetch the buffers from a different location.
	if ( type == REPLY_2_SEND ) {
		dest = to->a->send.reply;
		destLen = to->a->send.rpllen;
	}
	debug("passMessage: from (%d) [%d]bytes to (%d) [%d]bytes\r\n", from->id, sourceLen, to->id, destLen);

	// Verify the pointers point to valid memory addresses
	assert( isValidMem( source, sourceLen, from ) == NO_ERROR );
	assert( isValidMem( dest, destLen, to ) == NO_ERROR );

	// Copy the message over to this task 
	// Copy as much as we can and return the copied amount
	// User tasks should be able to handle this
	int len = sourceLen;
	int ret = len;

	if ( destLen < sourceLen ) {
		len = destLen;
		ret = (type == REPLY_2_SEND) ? RPLY_BUFFER_FULL : RCV_BUFFER_FULL;
	}
	
	// Do the actual copy
	memcpy ( dest, source, len );

	// Save the size of message copied, and state
	to->returnValue = len;
	to->state       = READY;

	switch( type ) {
		case SEND_2_RCV:
			// Cause the sender to wait for a reply
			from->state = RPLY_BLKD;
			break;
		case REPLY_2_SEND:
			// The receiver can continue operation
			from->state = READY;
			break;
	}
	return ret;
}

int awaitEvent (TD *td, TDM *mgr, int eventId ) {
	debug ("awaitEvent: tid:%d @(%x) eventId: %d\r\n", td->id, td, eventId);
	
	// Check that this is a valid event id
	if( eventId < 0 || eventId >= NUM_INTERRUPTS ) {
		return INVALID_EVENTID;
	}

	// Check that the driver for this event is installed
	if( mgr->intDriver[eventId] == 0 ) {
		debug ("awaitEvent: NO DRIVER HAS BEEN INSTALLED FOR THIS.\r\n");
		assert (1==0);
		return NO_DRIVER;
	}

	// Turn on this interrupt
	intr_set( eventId, ON );

	// Change task state
	td->state = AWAITING_EVT;
	
	// Put this task on the corresponding interrupt blocked queue
	queue_push ( &mgr->intBlocked[eventId], td );

	// Everything worked
	return NO_ERROR;
}

void handleInterrupt( TDM *mgr ) {
	
	// Get an index of the actual interrupt
	int eventId = intr_get(); 
	debug( "handleInterrupt: #%d status=%x\r\n", eventId );
	assert( eventId >= 0 );
	assert( eventId < NUM_INTERRUPTS );

	// Get the driver for the interrupt that happened
	Driver driver = mgr->intDriver[eventId];
	assert( driver != 0 );

	if( mgr->intBlocked[eventId] != 0 ) {
		// Pass the event to the task waiting for it
		TD* td = queue_pop( &mgr->intBlocked[eventId] );
		td->returnValue = driver( td->a->awaitEvent.event, 
				td->a->awaitEvent.eventLen );
		td->state = READY;
		mgr_insert( mgr, td );
	} 

	if( mgr->intBlocked[eventId] == 0 ) {
		// Turn off interrupts as there is no-one to handle them
		intr_set( eventId, OFF );
	}
	/*
	// Turn off software interrupts
	int *i = (int *) (VIC1_BASE + VIC_SOFT_INT_CLR);
	*i = 0xFFFFFFFF;
	*/
}
int destroy( TDM *mgr, TID tid ) {
	TD * victim = mgr_fetchById (mgr, tid);
	debug ("destroy: task @%x (%d)\r\n", victim, tid);

	// Check the tid
	if ( (int) victim < NO_ERROR ) {
		return (int) victim;
	}

	// Destroy the victim
	td_destroy( victim, mgr );


	return NO_ERROR;
}
