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

int checkStackAddr ( int *addr, TD *td ) {
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
int send (TD *sender, PQ *pq, TID tid) {
	// Check all arguments
	assert ( sender != 0 );
	assert ( pq != 0 );
	int ret;

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
		passMessage ( sender, receiver, pq );
	}
	else {
		// The receiver is not waiting for the sender yet.
		// Let's just wait on their send queue.
		;
	}

	return NO_ERROR;
}

// Returns.
// • The size of the message sent.
// • -1 – if the message was truncated.
int receive (TD *receiver, PQ *pq, TID *tid) {
	// Check all arguments
	assert ( receiver != 0 );
	assert ( pq != 0 );
	int ret;

	// Verify stack addresses are valid
	if ((ret = checkStackAddr((int *)receiver->args[0], receiver)) != NO_ERROR){
		return ret;
	}

	// Change the state to SEND_BLKD
	receiver->state = SEND_BLKD;

	// Check your send queue
	TD *sender = queue_pop ( &receiver->sendQ );
	
	// If someone is on it, complete the transaction.
	if ( sender != 0 ) {
		int ret = passMessage ( sender, receiver, pq );
		
		// Set the tid of the sender
		*tid = sender->id;

		if ( ret < 0 ) {
			return ret;
		}
	}
	else {
		// The sender has not sent our data yet.
		// Let's just wait.
		;
	}

	return NO_ERROR;
}

int passMessage ( TD *sender, TD *receiver, PQ *pq ) {
	// Verify sender and receiver states
	assert ( sender->state == RCV_BLKD );
	assert ( receiver->state == SEND_BLKD );
	int ret = NO_ERROR;
	
	// Get the message buffers and lengths
	char *sendBuffer = (char *) sender->args[0];
	int   sendBufLen = sender->args[1];
	
	char *recvBuffer = (char *) receiver->args[0];
	int   recvBufLen = receiver->args[1];

	// TODO: Yes, this is the second time we've checked this.
	// Verify the pointers point to valid memory addresses
	if ( (ret = checkStackAddr((int *) sendBuffer, sender)) != NO_ERROR ) {
		return ret;
	}
	if ( (ret = checkStackAddr((int *) recvBuffer, receiver)) != NO_ERROR ) {
		return ret;
	}

	// Copy the message over to this task 
	// Copy as much as we can and return the copied amount
	// User tasks should be able to handle this
	int len = ( sendBufLen > recvBufLen ) recvBufLen : sendBufLen;
	byteCopy ( len, sendBuffer, sendBufLen );

	// Set up the receiver
	receiver->returnValue = len;
	receiver->state = RPLY_BLKD;

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
	// FROM is really the receiver OR replier if a worker task was spawned
	// TO is really the sender that we are acking

	// Update the TD states
	TD *to = pq_fetchById ( tid );
	// pq_fetchById error checks the tid
	if ( to < 0 ) {
		return (int) to;
	}
	
	// If the sender is not reply blocked, we should error.
	if ( to->state != RPLY_BLKD ) {
		return SNDR_NOT_RPLY_BLKD;
	}

	// Copy the data to the "sender"
	int copyLen = (to->args[22] < rpllen) ? to->args[22] : rpllen; 
	byteCopy ( to->args[3], reply, copyLen );

	if ( to->args[22] < rpllen ) {
		return RPLY_BUFFER_FULL;
	}

	// Set up the return value
	to->returnValue = copyLen;

	// Set the state
	from->state = READY;
	to->state   = READY;

	// Make the tasks ready by putting them on the ready queues
	pq_insert ( pq, to );
	pq_insert ( pq, from );

	return 0;
}

