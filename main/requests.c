/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include "requests.h"
#include "switch.h"

#define SWI(n) asm("swi #" #n)

int Create (int priority, Task code ) {
	SWI(1);
	//return syscall(priority, (int) code, 0, CREATE);
}

int MyTid () {
	SWI(2);
//	return syscall(0, 0, 0, MYTID);
}

int MyParentTid() {
	SWI(3);
	//return syscall(0, 0, 0, MYPARENTTID);
}

void Pass () {
	SWI(4);
	//syscall(0, 0, 0, PASS);
}

void Exit () {
	SWI(5);
	//syscall(0, 0, 0, EXIT);
}

// This is what a task calls.
int Send (int tid, char *msg, int msglen, char *reply, int rpllen) {
	SWI(6);
}


int Receive (int *tid, char *msg, int msglen) {
	SWI(7);
}


int Reply (int tid, char *reply, int rpllen) {
	SWI(8);
}


int RegisterAs (char *name) {
	SWI(9);
}


int WhoIs (char *name) {
	SWI(10);
}


// This is what the kernel calls.
//
// RETURNS:
//• The size of the message supplied by the replying task.
//• -1 – if the task id is impossible.
//• -2 – if the task id is not an existing task.
//• -3 – if the send-receive-reply transaction is incomplete.
int send (int tid, char *msg, int msglen, char *reply, int rpllen) {
	// Check all arguments
	
	// Copy all arguments to the user space.

	// Set state to RCV_BLCK
	
	// Put yourself on the other task's send queue.

	// Is the Receiver SEND_BLCKed?

	// If not, exit

	// Else, do all of the work yourself
}

// Returns.
// • The size of the message sent.
// • -1 – if the message was truncated.
int receive (int *tid, char *msg, int msglen) {
	// Check all arguments

	// Change the state to SEND_BLCK

	// Check your send queue

	// If someone is on it, complete the transaction.

		// THE FOLLOWING should go into a function so that it can be called from
		// Send and Recieve
		
		// Get the sender
		// Verify sender is RCV_BLCK
		// Copy the message over to this task - remember to check the size
		// Change the receiver state to READY
		// Put the receiver on ready queue
		// Set up return value
	
	// Else, you can't complete it so exit.
}

/*
 * Returns.
  • 0 – if the reply succeeds.
  • -1 – if the task id is not a possible task id.
  • -2 – if the task id is not an existing task.
  • -3 – if the task is not reply blocked.
  • -4 – if there was insufficient space for the entire reply in the sender’s reply buffer.
*/
int reply (int tid, char *reply, int rpllen) {


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
