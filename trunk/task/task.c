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
#include "clockserver.h"
#include "gameplayer.h"
#include "gameserver.h"
#include "nameserver.h"
#include "requests.h"
#include "task.h"

typedef struct {
	char delayLen;
	char numDelays;
} clientMsg;
//-------------------------------------------------------------------------
//---------------------------Kernel 3--------------------------------------
//-------------------------------------------------------------------------
void k3_firstUserTask () {
	debug ("First task started. \r\n");
	int len;
	TID id;
	char req;
	clientMsg msgs[4];

	// Create the name server
	debug ("Creating the name server. \r\n");
	Create (1, &ns_run);

	// Create the clock server
	debug ("Creating the clock server. \r\n");
	Create (1, &cs_run);

	// Create the clients
	Create (3, &k3_client);
	Create (4, &k3_client);
	Create (5, &k3_client);
	Create (6, &k3_client);
	
	// Initialize the return values for each client
	msgs[0].delayLen = 10;
	msgs[0].numDelays = 20;
	msgs[1].delayLen = 23;
	msgs[1].numDelays = 9;
	msgs[2].delayLen = 33;
	msgs[2].numDelays = 6;
	msgs[3].delayLen = 71;
	msgs[3].numDelays = 3;

	// Receive from the clients
	debug ("First user task is receiving.\r\n");
	int i;
	for ( i = 0; i < 4; i ++ ) {
		len = Receive (&id, (char*) &req, sizeof(char));
		assert ( len == sizeof(char) );
	
		Reply ( id, (char *) &msgs[i], sizeof(clientMsg) );
	}
	
	// Quit since this task is done
	debug ("First user task exiting.\r\n");
	Exit();

}

void k3_client () {
	debug ("Client %d is starting.\r\n", MyTid());
	clientMsg msg;
	char blank = 0;
	int csTid = WhoIs (CLOCK_NAME);
	int i;

	// Register with the name server
	RegisterAs ("Client" + MyTid());
	
	// Send to your parents
	Send( MyParentTid(), &blank, 1, (char*) &msg, sizeof(clientMsg) );
	
	// Delay the returned number of times and print.
	debug("delayLen= %d, numDelays = %d\r\n", msg.delayLen, msg.numDelays );
	for ( i = 0; i < msg.numDelays; i ++ ) {
		Delay (msg.delayLen, csTid);
		bwprintf (COM2, "Tid: %d \t Delay Interval: %d \t Num of Completed Delays: %d \r\n", 
				MyTid(), msg.delayLen, i+1);
	}

	// Exit!
	debug ("Client %d is exiting.\r\n", MyTid());
	Exit();
}

//-------------------------------------------------------------------------
//---------------------------Kernel 2--------------------------------------
//-------------------------------------------------------------------------

void k2_firstUserTask () {
	debug ("First task started. \r\n");

	// Create the name server
	debug ("Creating the name server. \r\n");
	Create (1, &ns_run);

	// Create the game server for Rock Paper Scissors
	debug ("Creating the Rock Paper Scissors server. \r\n");
	Create (1, &gs_run);
//	Send( 1, 2, 3, 4, 5 );

	// Create players for the game server
	debug ("Creating 8 players. \r\n");
	
	// Same priorities
	Create (2, &rockPlayer);
	Create (2, &clonePlayer);
	
/*	Create (2, &paperPlayer);
	Create (2, &clonePlayer);
*/	
	// 2 at same priority
	Create (1, &scissorsPlayer);
	Create (1, &clonePlayer);
	
	// 2 at different priorities
/*	Create (3, &clonePlayer);
	Create (4, &clonePlayer);
*/
	int *i = (int *) (VIC1_BASE + VIC_SOFT_INT);
	int k, a=0;
	for(k = 0; k < 25; k++ ) {
		bwprintf(COM2, "sending software int to addr:%x. \r\n", i);
		a = 0;
		*i = 0x10;
		asm("#;after here");
		a += 1;
		a += 2;
		a += 4;
		a += 8;
		a += 16;
		a += 32;
		a += 64;
		a += 128;

		bwprintf(COM2, "sent a software int. waiting for input, a=%x\r\n", a);
		bwgetc(COM2);
	}

/*
	// Let's make 50 players to test the td recycling
	debug ("Creating 50 players at the lowest priority. \r\n");
	int i;
	for (i = 0; i < 50; i += 5) {
		Create ((i%5)+5, &robinPlayer);
		Create ((i%5)+5, &clonePlayer);
		Create ((i%5)+5, &robinPlayer);
		Create ((i%5)+5, &clonePlayer);
		Create ((i%5)+5, &robinPlayer);
	}
*/
	// Quit since our work is done.
	debug ("First user task exiting. Enjoy the games!\r\n");
	Exit();
}

void k2_registerTask1 () {
	debug ("First task started. \r\n");

	// Create the name server
	debug ("Creating the name server. \r\n");
	Create (1, &ns_run);
	Create (2, &k2_registerTask2);
	Create (1, &k2_registerTask2);

	RegisterAs ("A");
	assert ( WhoIs ("A") == MyTid() );
	RegisterAs ("B");
	assert ( WhoIs ("B") == MyTid() );
	RegisterAs ("C");
	assert ( WhoIs ("C") == MyTid() );
	RegisterAs ("D");
	assert ( WhoIs ("D") == MyTid() );
	RegisterAs ("D");
	assert ( WhoIs ("D") == MyTid() );
	RegisterAs ("E");
	assert ( WhoIs ("E") == MyTid() );

	Create (0, &k2_registerTask2);
	debug ("First task is exiting.\r\n");
	Exit();
}

void k2_registerTask2 () {
	RegisterAs ("Barbara");
	assert ( WhoIs ("Barbara") == MyTid() );
	
	RegisterAs ("Macdonald");
	assert ( WhoIs ("Macdonald") == MyTid() );
	
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
