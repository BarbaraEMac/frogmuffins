/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
//#define DEBUG
#include <bwio.h>
#include <ts7200.h>
#include <string.h>

#include "debug.h"
#include "error.h"
#include "syscalls.h"
#include "td.h"

int isValidMem ( const char *addr, const TD *td ) {
	if ( addr == 0 ) {
		return NULL_ADDR;
	}

	if ( ((int *) addr >= td->sb) || ((int*) addr < td->sb-STACK_SIZE) ) {
		return OUT_OF_BOUNDS;
	}

	return NO_ERROR;
}

int send (TD *sender, PQ *pq, TID tid) {
	debug ("send: from=%x, to=%x (%d)\r\n", sender, pq_fetchById(pq, tid), tid);
	// Check all arguments
	assert ( sender != 0 );
	assert ( sender->state == ACTIVE );
	assert ( pq != 0 );
	assert ( sender->id != tid ); // Do not send a message to yourself.
	int ret = NO_ERROR;

	// Verify the pointers point to valid memory addresses
	if ( (ret = isValidMem(sender->a->send.msg, sender)) != NO_ERROR ) {
		return ret;
	}
	if ( (ret = isValidMem(sender->a->send.reply, sender)) != NO_ERROR ) {
		return ret; // TODO Add a mask here so that we can tell is apart
	}
	
	TD *receiver = pq_fetchById (pq, tid);
	// pq_fetchById error checks the tid
	if ( receiver < NO_ERROR ) {
		return (int) receiver;
	}
	
	// Set state to receive blocked
	sender->state = RCV_BLKD;

	// Is the receiver send blocked?
	if ( receiver->state == SEND_BLKD ) {
		// Verify sender and receiver states
		assert ( sender->state == RCV_BLKD );
		assert ( receiver->state == SEND_BLKD );
		
		ret = passMessage ( sender, receiver, SEND_2_RCV );
		assert (sender->state == RPLY_BLKD);

		// Pass the sender tid to the receiver
		*receiver->a->receive.tid = sender->id;

		// Unblock the receiver
		pq_insert(pq, receiver);
	} else {
		// Put yourself on the other task's send queue.
		debug ("PUSHING %x ON A SEND Q st=%d\r\n", sender, sender->state);
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
	if ((ret = isValidMem(receiver->a->receive.msg, receiver)) != NO_ERROR ){
		return ret;
	}

	// Change the state to SEND_BLKD
	receiver->state = SEND_BLKD;

	// If someone is on the send queue, complete the transaction.
	if ( receiver->sendQ != 0 ) {
		TD *sender = queue_pop ( &receiver->sendQ );
		debug ("POPPING %x OFF A SEND Q st=%d\r\n", sender, sender->state);

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

int reply (TD *from, PQ *pq, TID tid, char *reply, int rpllen) {
	debug ("rply: from=%x (%d) to=%x (%d) \r\n", from, from->id, pq_fetchById(pq, tid), tid);
	assert ( from != 0 );
	assert ( pq != 0 );
	assert ( from->id != tid ); // Do not reply to yourself.
	int ret = NO_ERROR;

	// FROM is really the receiver OR replier if a worker task was spawned
	// TO is really the sender that we are acking

	// Update the TD states
	TD *to = pq_fetchById ( pq, tid );
	// pq_fetchById error checks the tid
	if ( to < NO_ERROR ) {
		return (int) to;
	}
	
	// If the sender is not reply blocked, we should error.
	assert (to->state == RPLY_BLKD);
	if ( to->state != RPLY_BLKD ) {
		return SNDR_NOT_RPLY_BLKD;
	}
	
	// Check the msg pointer
	if ((ret = isValidMem(from->a->reply.reply, from)) != NO_ERROR ){
		return ret;
	}

	// Copy the data over
	ret = passMessage ( from, to, REPLY_2_SEND );
	// Even if PassMessage failed keep going
	assert (to->state == READY);
	assert (from->state == READY);

	// Make the sender (to) task ready by putting it on a ready queue
	// NOTE: 'from' will be put on queue by send() if it was blocked
	pq_insert ( pq, to );

	return ret;
}

/*
 * The arguments passed in should already be checked
 * this method will return BUFFER_FULL errors if the source buffer
 * lenght is longer than the destination
 * Updates the states of the tasks
 */
int passMessage ( TD *from, TD *to, MsgType type ) {
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
	assert( isValidMem(source, from) == NO_ERROR );
	assert( isValidMem(dest, to) == NO_ERROR );

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
