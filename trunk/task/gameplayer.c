/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * A Rock, Paper, Scissors Client / Player
 */

#define DEBUG
#define	MSG_LEN		1024

#include <bwio.h>
#include <ts7200.h>

#include "debug.h"
#include "globals.h"
#include "requests.h"

#include "gameplayer.h"

#define BUFFER_LEN		1024

void genericPlayer (char *name, GameMove move, int timesToPlay) {
	int i;
	int myTid = MyTid();
	char rplyBuffer[1024];
	int gameServer = WhoIs ("GameServer");
	ServerReply reply;
	// Initialize this player's request to be sent to the game server
	PlayerRequest req;
	req.name = name;
	req.move = move;

	// Register with the name server
	bwprintf (COM2, "Player: Registering as \"%s\"(%d).\r\n", name, myTid);
	RegisterAs (name);
	assert ( WhoIs(name) == MyTid() );

	// Sign up for a game
	bwprintf (COM2, "Player %s (%d): Signing up for a game.\r\n", name, myTid);
	req.type = SIGNUP;
	Send (gameServer, (char*)&req, sizeof(PlayerRequest), rplyBuffer, BUFFER_LEN);
	
	// Play the game
	req.type = PLAY;
	for ( i = 0; i < timesToPlay; i ++ ) {
		bwprintf (COM2, "Player %s (%d): Playing %c.\r\n", name, myTid, move);
		Send (gameServer, (char*)&req, sizeof(PlayerRequest), rplyBuffer, BUFFER_LEN);

		// TODO: Remove this? Copy straight into this?
		// Process the server's reply
		memcpy ( (char*)&reply, rplyBuffer, sizeof(ServerReply) );
		
		// Print the appropriate message.
		switch ( reply.result ) {
			case WIN:
				bwprintf (COM2, "Player %s (%d): Won against %s (%d)!\r\n", 
						  name, myTid, reply.opponent, WhoIs(reply.opponent));
				break;
			
			case LOSE:
				bwprintf (COM2, "Player %s (%d): Lost to %s (%d) :\(.\r\n", 
						  name, myTid, reply.opponent, WhoIs(reply.opponent));
				break;
			
			case TIE:
				bwprintf (COM2, "Player %s (%d): Tied with %s (%d).\r\n", 
						  name, myTid, reply.opponent, WhoIs(reply.opponent));
				break;

			default:
				assert ( 1 == 0 ); // You should NEVER get here
				break;
		}
	}

	// This player is bored. Quit playing.
	bwprintf (COM2, "Player %s (%d): Quitting.r\n", name, myTid);
	req.type = QUIT;
	Send (gameServer, (char*)&req, sizeof(PlayerRequest), rplyBuffer, BUFFER_LEN);
}

// Only plays rock
void rockPlayer () {
	char *name = "Rocky";
	
	genericPlayer (name, ROCK, 3);

	Exit();
}

// Only plays scissors
void scissorsPlayer () {
	char *name = "Edward Scis";

	genericPlayer (name, SCISSORS, 3);

	Exit();
}

// Only plays paper
void paperPlayer () {
	char *name = "Paper Clip";
	
	genericPlayer (name, PAPER, 3);

	Exit();
}
