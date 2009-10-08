/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * User task stuff goes in here.
 */

#define DEBUG
#include <bwio.h>
#include <ts7200.h>

#include "debug.h"
#include "gameplayer.h"
#include "gameserver.h"
#include "nameserver.h"
#include "requests.h"
#include "task.h"

//-------------------------------------------------------------------------
//---------------------------Kernel 2--------------------------------------
//-------------------------------------------------------------------------
void k2_firstUserTask () {
	debug ("First task started> \r\n");

	// Create the name server
	Create (1, &ns_run);

	// Create the game server for Rock Paper Scissors
	Create (1, &gameserver_run);

	// Create players for the game server
	Create (2, &rockPlayer);
	Create (2, &paperPlayer);

	Exit();
}

void k2_senderTask () {

    bwputstr (COM2, "Sender: starting.\r\n");
	char msg[] = "I CAN HAS", reply[12];
	int len;
	len = Send(0, msg, sizeof(msg), reply, sizeof(reply));
	bwprintf (COM2, "Received reply: '%s' of length: %d.\r\n", reply, len);

    bwputstr (COM2, "Sender: exiting.\r\n");
    Exit();

}

void k2_receiverTask () {
    bwputstr (COM2, "Receiver: starting.\r\n");
    bwprintf (COM2, "Created: %d.\r\n", Create (0, &k2_senderTask)); 
	char msg[12], reply[]="KTHXBAI";
	int len, tid;
    bwputstr (COM2, "Receiver: listening.\r\n");
	len = Receive(&tid, msg, sizeof(msg));
	bwprintf (COM2, "Received msg: '%s' of length: %d from tid: %d.\r\n", msg, len, tid);
	len = Reply(tid, reply, sizeof(reply));

    bwprintf (COM2, "Receiver: reply to tid: %d with result %d, exiting.\r\n", tid, len);
    Exit();
}

//-------------------------------------------------------------------------
//---------------------------Kernel 1--------------------------------------
//-------------------------------------------------------------------------

/*
 * The first user task runs this function.
 */
void k1_firstTaskStart () {
	debug( "First task started. \r\n");

    // Create low priority
    bwprintf (COM2, "Created: %d.\r\n", Create (2, &k1_userTaskStart)); 
    bwprintf (COM2, "Created: %d.\r\n", Create (2, &k1_userTaskStart)); 
    
	// Create high priority
    bwprintf (COM2, "Created: %d.\r\n", Create (0, &k1_userTaskStart)); 
    bwprintf (COM2, "Created: %d.\r\n", Create (0, &k1_userTaskStart)); 

	// Exit
    bwputstr (COM2, "First: exiting.\r\n");
    Exit();
}

/**
 * After the first user task, all following tasks run this function.
 */
void k1_userTaskStart ( ) {
    
	// Print your information
    bwprintf (COM2, "Tid: %d Parent Tid: %d\r\n", MyTid(), MyParentTid());

	// Let someone else run
    Pass();
    
	// Print your information again
    bwprintf (COM2, "Tid: %d Parent Tid: %d\r\n", MyTid(), MyParentTid());

    Exit();
}
