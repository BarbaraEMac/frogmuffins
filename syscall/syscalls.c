/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#define DEBUG
#include <bwio.h>
#include <ts7200.h>

#include "debug.h"
#include "error.h"
#include "syscalls.h"
#include "td.h"

void byteCopy ( char *dest, const char *source, int len ) {
	while( (--len) >= 0 ) dest[len] = source[len];
}

int checkStackAddr ( const int *addr, const TD *td ) {
	if ( addr == 0 ) {
		return NULL_ADDR;
	}

	if ( (addr >= td->sb) || (addr < td->sb-STACK_SIZE) ) {
		return OUT_OF_BOUNDS;
	}

	return NO_ERROR;
}
//
// RETURNS:
//• The size of the message supplied by the replying task.
//• -1 – if the task id is impossible.
//• -2 – if the task id is not an existing task.
//• -3 – if the send-receive-reply transaction is incomplete.
int send (TD *sender, const PQ *pq, TID tid) {
	// Check all arguments
	assert ( sender != 0 );
	assert ( pq != 0 );
	int ret = NO_ERROR;

	// TODO: REmove this since we check in passMessage?
	// Verify the pointers point to valid memory addresses
	if ( (ret = checkStackAddr((int *)sender->args[0], sender)) != NO_ERROR ) {
		return ret;
	}
	if ( (ret = checkStackAddr((int *)sender->args[3], sender)) != NO_ERROR ) {
		return ret; // TODO Add a mask here so that we can tell is apart
	}
	
	TD *receiver = pq_fetchById (pq, tid);
	// pq_fetchById error checks the tid
	if ( receiver < 0 ) {
		return (int) receiver;
	}
	
	// Copy all arguments to the user space.
	// Apparently this has already been done in the context switch ....

	// Set state to RCV_BLCK
	sender->state = RCV_BLKD;

	// Put yourself on the other task's send queue.
	queue_push ( &receiver->sendQ, sender );

	// Is the Receiver SEND_BLCKed?
	if ( receiver->state == SEND_BLKD ) {
		// Verify sender and receiver states
		assert ( sender->state == RCV_BLKD );
		assert ( receiver->state == SEND_BLKD );
		
		ret = passMessage ( sender, receiver, &receiver->returnValue, 
							0 /*not a reply*/ );
		
		// Cause the sender to wait for a reply
		sender->state = RPLY_BLKD;
	}
	else {
		// The receiver is not waiting for the sender yet.
		// Let's just wait on their send queue.
		;
	}

	return ret;
}

// Returns.
// • The size of the message sent.
// • -1 – if the message was truncated.
int receive (TD *receiver, TID *tid) {
	// Check all arguments
	assert ( receiver != 0 );
	int ret;

	// Verify stack addresses are valid
	// TODO: REmove this since we check in passMessage?
	if ((ret = checkStackAddr((int *)receiver->args[0], receiver)) != NO_ERROR){
		return ret;
	}

	// Change the state to SEND_BLKD
	receiver->state = SEND_BLKD;

	// Check your send queue
	TD *sender = queue_pop ( &receiver->sendQ );
	
	// If someone is on it, complete the transaction.
	if ( sender != 0 ) {
		// Verify sender and receiver states
		assert ( sender->state == RCV_BLKD );
		assert ( receiver->state == SEND_BLKD );
		
		ret = passMessage ( sender, receiver, &receiver->returnValue, 
							0/*not a reply*/ );
		
		// Set the tid of the sender
		*tid = sender->id;
		
		// Cause the sender to wait for a reply
		sender->state = RPLY_BLKD;
	}
	else {
		// The sender has not sent our data yet.
		// Let's just wait.
		;
	}

	return ret;
}

/*
 * Returns.
  •  0 – if the reply succeeds.
  • -1 – if the task id is not a possible task id.
  • -2 – if the task id is not an existing task.
  • -3 – if the task is not reply blocked.
  • -4 – if there was insufficient space for the entire reply in the sender’s reply buffer.
*/
int reply (TD *from, PQ *pq, TID tid, char *reply, int rpllen) {
	assert ( from != 0 );
	assert ( pq != 0 );
	assert ( from->state == SEND_BLKD );

	// FROM is really the receiver OR replier if a worker task was spawned
	// TO is really the sender that we are acking

	// Update the TD states
	TD *to = pq_fetchById ( pq, tid );
	// pq_fetchById error checks the tid
	if ( to < 0 ) {
		return (int) to;
	}
	
	// If the sender is not reply blocked, we should error.
	if ( to->state != RPLY_BLKD ) {
		return SNDR_NOT_RPLY_BLKD;
	}
	
	// Copy the data over
	int ret = passMessage ( to, from, &to->returnValue, 1 /*reply*/ );

	// TODO: If something in passMessage failed, still take off queues!? Probably ....
	if ( ret < 0 ) {
		return ret;
	}

	// Set the state
	from->state = READY;
	to->state   = READY;

	// Make the tasks ready by putting them on the ready queues
	pq_insert ( pq, to );
	pq_insert ( pq, from );

	return NO_ERROR;
}

int passMessage ( const TD *from, const TD *to, int *copyLen, int rply ) {
	int ret = NO_ERROR;
	int toArg1 = 0, toArg2 = 1;

	// If we are doing a reply, fetch the buffers from a different location.
	if ( rply == 1 ) {
		toArg1 = 3;
		toArg2 = 22;
	}

	// Get the message buffers and lengths
	char *fromBuffer = (char *) from->args[0];
	int   fromBufLen = from->args[1];
	
	char *toBuffer = (char *) to->args[toArg1];
	int   toBufLen = to->args[toArg2];

	// TODO: Yes, this is the second time we've checked this.
	// Verify the pointers point to valid memory addresses
	if ( (ret = checkStackAddr((int *) fromBuffer, from)) != NO_ERROR ) {
		return ret;
	}
	if ( (ret = checkStackAddr((int *) toBuffer, to)) != NO_ERROR ) {
		return ret;
	}

	// Copy the message over to this task 
	// Copy as much as we can and return the copied amount
	// User tasks should be able to handle this
	*copyLen = fromBufLen;
	
	if ( toBufLen < fromBufLen ) {
		*copyLen = toBufLen;
		ret = rply ? RPLY_BUFFER_FULL : RCV_BUFFER_FULL;
	}
	
	byteCopy ( toBuffer, fromBuffer, *copyLen );

	return ret;
}
