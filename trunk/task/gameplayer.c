/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * A Rock, Paper, Scissors Client / Player
 */

#define DEBUG

#include <bwio.h>
#include <ts7200.h>
#include <string.h>

#include "debug.h"
#include "globals.h"
#include "requests.h"

#include "gameplayer.h"

void genericPlayer (char *name, GameMove move, int timesToPlay) {
	int i;
	int myTid = MyTid();
	int gameServer = WhoIs ("GameServer");
	ServerReply reply;
	// Initialize this player's request to be sent to the game server
	PlayerRequest req;
	strncpy(req.name, name, sizeof(req.name));
	req.move = move;

	// Register with the name server
	bwprintf (COM2, "Player: Registering as \"%s\"(%d).\r\n", name, myTid);
	RegisterAs (name);
	assert ( WhoIs(name) == MyTid() );

	// Sign up for a game
	bwprintf (COM2, "Player: %s (%d): Signing up for a game.\r\n", name, myTid);
	req.type = SIGNUP;
	Send (gameServer, (char*)&req, sizeof(PlayerRequest), (char*)&reply, sizeof(ServerReply));
	
	// Play the game
	req.type = PLAY;
	for ( i = 0; i < timesToPlay; i ++ ) {
		bwprintf (COM2, "Player: %s (%d): Playing %c.\r\n", name, myTid, move);
		Send (gameServer, (char*)&req, sizeof(PlayerRequest), (char*)&reply, sizeof(ServerReply));

		bwgetc(COM2);

		// Process the server's reply
		// Print the appropriate message.
		switch ( reply.result ) {
			case WIN:
				bwprintf (COM2, "Player: %s (%d): Won against %s (%d)!\r\n", 
						  name, myTid, reply.opponent.name, reply.opponent.tid);
				break;
			
			case LOSE:
				bwprintf (COM2, "Player: %s (%d): Lost to %s (%d) :\(.\r\n", 
						  name, myTid, reply.opponent.name, reply.opponent.tid);
				break;
			
			case TIE:
				bwprintf (COM2, "Player: %s (%d): Tied with %s (%d).\r\n", 
						  name, myTid, reply.opponent.name, reply.opponent.tid);
				break;

			default:
				assert ( 1 == 0 ); // You should NEVER get here
				break;
		}
	}

	// This player is bored. Quit playing.
	bwprintf (COM2, "Player: %s (%d): Quitting.\r\n", name, myTid);
	req.type = QUIT;
	Send (gameServer, (char*)&req, sizeof(PlayerRequest), (char*)&reply, sizeof(ServerReply));

	// Exit the player
	Exit();
}

// Only plays rock
void rockPlayer () {
	debug ("Rock player is starting. \r\n");
	char *name = "Dwayne J.";
	
	genericPlayer (name, ROCK, 3);
}
// Only plays scissors
void scissorsPlayer () {
	debug ("Scissors player is starting. \r\n");
	char *name = "Edward S.";

	genericPlayer (name, SCISSORS, 3);
}

// Only plays paper
void paperPlayer () {
	debug ("Paper player is starting. \r\n");
	char *name = "Paper Mario";
	
	genericPlayer (name, PAPER, 3);
}
