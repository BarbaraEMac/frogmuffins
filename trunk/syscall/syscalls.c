/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

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
	assert ( td != 0 );
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
	assert ( td != 0 );
	assert ( pq != 0 );
	int ret;

	// Verify stack addresses are valid
	if ( (ret = checkStackAddr((int *)receiver->args[0], receiver)) != NO_ERROR ) {
		return ret;
	}

	// Change the state to SEND_BLCK
	receiver->state = SEND_BLKD;

	// Check your send queue
	TD *sender = queue_pop ( &receiver->sendQ );
	
	// If someone is on it, complete the transaction.
	if ( sender != 0 ) {
		passMessage ( sender, receiver, pq );
		
		// Set the tid of the sender
		*tid = sender->id;
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
	int ret;
	
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
	// TODO: Copy as much as we can and return the copied amount
	// User tasks should be able to handle this
	if ( sendBufLen > recvBufLen ) {
		return RCV_BUFFER_FULL;
	} else {
		byteCopy ( recvBuffer, sendBuffer, sendBufLen );
	}

	// Change the receiver state to READY
	receiver->state = READY;

	// Put the receiver on ready queue
	pq_insert (pq, receiver);

	// Set up return value

	return 0;
}


/*
 * Returns.
  • 0 – if the reply succeeds.
  • -1 – if the task id is not a possible task id.
  • -2 – if the task id is not an existing task.
  • -3 – if the task is not reply blocked.
  • -4 – if there was insufficient space for the entire reply in the sender’s reply buffer.
*/
int reply (TD *sender, PQ *pq, TID tid, char *reply, int rpllen) {

	// Update the TD states
	//

	// Unblock sender first


	// Unblock receiver second
}

/*
 * Returns.
 • 0 – success.
 • -1 – if the nameserver task id inside the wrapper is invalid.
 • -2 – if the nameserver task id inside the wrapper is not the name
    server.
 */
int registerAs (char *name) {

}

/*
Returns.
 • tid – the task id of the registered task.
 • -1 – if the nameserver task id inside the wrapper is invalid.
 • -2 – if the nameserver task id inside the wrapper is not the
    nameserver.
*/
int whoIs (char *name) {

}
