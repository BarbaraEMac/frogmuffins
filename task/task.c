/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * User task stuff goes in here.
 */

#define DEBUG 1

#include <ts7200.h>

#include "debug.h"
#include "gameplayer.h"
#include "gameserver.h"
#include "requests.h"
#include "servers.h"
#include "task.h"


//-------------------------------------------------------------------------
//---------------------------Kernel 4--------------------------------------
//-------------------------------------------------------------------------

// NOTE: Kernel 4 is mostly located with in shell.c


//-------------------------------------------------------------------------
//---------------------------Kernel 3--------------------------------------
//-------------------------------------------------------------------------
typedef struct {
	char delayLen;
	char numDelays;
} clientMsg;

void k3_firstUserTask () {
	debug ("First task started. \r\n");
	int 	  len;
	TID 	  id;
	char 	  req;
	clientMsg msgs[4];

	// The Shell will make the Name and Clock servers

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
		len = Receive (&id, &req, sizeof(char));
		assert ( len == sizeof(char) );
		Reply ( id, (char *) &msgs[i], sizeof(clientMsg) );
	}
	
//	WaitFor ( 4 );
	// Quit since this task is done
	debug ("First user task exiting.\r\n");
	Exit();

}

void k3_client () {
	debug ("Client%d is starting.\r\n", MyTid());
	clientMsg msg;
	char blank = 0;
	int csTid = WhoIs (CLOCK_NAME);
	int i;

	// Register with the name server
//	RegisterAs ("Client" + MyTid()); we don't have string concatenation
	
	// Send to your parent
	Send( MyParentTid(), &blank, 1, (char*) &msg, sizeof(clientMsg) );
	
	// Delay the returned number of times and print.
	debug("delayLen= %d, numDelays = %d\r\n", msg.delayLen, msg.numDelays );
	for ( i = 0; i < msg.numDelays; i ++ ) {
		Delay (msg.delayLen, csTid);
		printf( "Tid: %d \t Delay Interval: %d \t Num of Completed Delays: %d \r\n", 
				MyTid(), msg.delayLen, i+1);
	}

//	Sync( MyParentTid() );
	// Exit!
	debug ("Client %d is exiting.\r\n", MyTid());
	Exit();
}

//-------------------------------------------------------------------------
//---------------------------Kernel 2--------------------------------------
//-------------------------------------------------------------------------

void k2_firstUserTask () {
	debug ("First task started. \r\n");

	// The Shell will make the Name Server

	// Create the game server for Rock Paper Scissors
	debug ("Creating the Rock Paper Scissors server. \r\n");
	Create (1, &gs_run);

	// Create players for the game server
	debug ("Creating 8 players. \r\n");
	
	// Same priorities
	Create (4, &rockPlayer);
	Create (4, &clonePlayer);
	
	Create (4, &paperPlayer);
	Create (4, &clonePlayer);
	
	// 2 at same priority
	Create (3, &scissorsPlayer);
	Create (3, &clonePlayer);
	
	// 2 at different priorities
	Create (5, &clonePlayer);
	Create (6, &clonePlayer);

	// Let's make 50 players to test the td recycling
	debug ("Creating 50 players at the lowest priority. \r\n");
	int i;
	for (i = 0; i < 40; i += 5) {
		Create ((i%5)+5, &robinPlayer);
		Create ((i%5)+5, &clonePlayer);
		Create ((i%5)+5, &robinPlayer);
		Create ((i%5)+5, &clonePlayer);
		Create ((i%5)+5, &robinPlayer);
	}

	// Quit since our work is done.
	debug ("First user task exiting. Enjoy the games!\r\n");
	Exit();
}

void k2_registerTask1 () {
	debug ("First task started. \r\n");

	// The Shell will make the Name Server
	
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

    printf( "Sender: starting.\r\n");
	char msg[] = "I CAN HAS", reply[12];
	int len;
	len = Send(0, msg, sizeof(msg), reply, sizeof(reply));
	printf( "Received reply: '%s' of length: %d.\r\n", reply, len);

    printf( "Sender: exiting.\r\n");
    Exit();

}

void k2_receiverTask () {
    printf( "Receiver: starting.\r\n");
    printf( "Created: %d.\r\n", Create (0, &k2_senderTask)); 
	char msg[12], reply[]="KTHXBAI";
	int len, tid;
    printf( "Receiver: listening.\r\n");
	len = Receive(&tid, msg, sizeof(msg));
	printf( "Received msg: '%s' of length: %d from tid: %d.\r\n", msg, len, tid);
	len = Reply(tid, reply, sizeof(reply));

    printf( "Receiver: reply to tid: %d with result %d, exiting.\r\n", tid, len);
    Exit();
}

//-------------------------------------------------------------------------
//---------------------------Kernel 1--------------------------------------
//-------------------------------------------------------------------------

/*
 * The first user task runs this function.
 */
void k1_firstUserTask () {
	debug( "First task started. \r\n");

    // Create low priority
    printf( "Created: %d.\r\n", Create (2, &k1_userTaskStart)); 
    printf( "Created: %d.\r\n", Create (2, &k1_userTaskStart)); 
    
	// Create high priority
    printf( "Created: %d.\r\n", Create (0, &k1_userTaskStart)); 
    printf( "Created: %d.\r\n", Create (0, &k1_userTaskStart)); 

	// Exit
    printf( "First: exiting.\r\n");
    Exit();
}

/**
 * After the first user task, all following tasks run this function.
 */
void k1_userTaskStart ( ) {
    
	// Print your information
    printf( "Tid: %d Parent Tid: %d\r\n", MyTid(), MyParentTid());

	// Let someone else run
    Pass();
    
	// Print your information again
    printf( "Tid: %d Parent Tid: %d\r\n", MyTid(), MyParentTid());

    Exit();
}
