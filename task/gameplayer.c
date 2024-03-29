/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * A Rock, Paper, Scissors Client / Player
 */

//#define DEBUG

#include <ts7200.h>
#include <string.h>

#include "debug.h"
#include "globals.h"
#include "requests.h"
#include "servers.h"

#include "gameplayer.h"


void genericPlayer (char *name, GameMove start, NextMove getNext, int timesToPlay) {
	int i;
	int myTid = MyTid();
	int gameServer = WhoIs ("GameServer");
	ServerReply reply;
	// Initialize this player's request to be sent to the game server
	PlayerRequest req;
	strncpy(req.name, name, sizeof(req.name));
	req.move = start;

	// Register with the name server
	debug ("Player: Registering as \"%s\"(%d).\r\n", req.name, myTid);
	RegisterAs (name);
	assert ( WhoIs(name) == MyTid() );

	// Sign up for a game
	printf ( "Player: %s (%d): Signing up for a game.\r\n", name, myTid);
	req.type = SIGNUP;
	Send (gameServer, (char*)&req, sizeof(PlayerRequest), (char*)&reply, sizeof(ServerReply));
	
	// Play the game
	req.type = PLAY;
	for ( i = 0; i < timesToPlay; i ++ ) {
		printf ( "Player: %s (%d): Playing %c.\r\n", name, myTid, req.move);
		Send (gameServer, (char*)&req, sizeof(PlayerRequest), (char*)&reply, sizeof(ServerReply));

		// Process the server's reply
		// Print the appropriate message.
		switch ( reply.result ) {
			case WIN:
				printf ( "Player: %s (%d): Won against %s (%d)!\r\n", 
						  name, myTid, reply.opponent.name, reply.opponent.tid);
				break;
			
			case LOSE:
				printf ( "Player: %s (%d): Lost to %s (%d) :\(.\r\n", 
						  name, myTid, reply.opponent.name, reply.opponent.tid);
				break;
			
			case TIE:
				printf ( "Player: %s (%d): Tied with %s (%d).\r\n", 
						  name, myTid, reply.opponent.name, reply.opponent.tid);
				break;

			default:
				assert ( 1 == 0 ); // You should NEVER get here
				break;
		}
		req.move = getNext(req.move, reply.opponent.move);
	}

	// This player is bored. Quit playing.
	printf ( "Player: %s (%d): Quitting.\r\n", name, myTid);
	req.type = QUIT;
	Send (gameServer, (char*)&req, sizeof(PlayerRequest), (char*)&reply, sizeof(ServerReply));

	printf ( "Player: %s (%d): Quit.\r\n", name, myTid);
	// Exit the player
	Exit();
}

GameMove repeat( GameMove mine, GameMove theirs ) {
	return mine;
}
GameMove mirror( GameMove mine, GameMove theirs ) {
	return theirs;
}
GameMove predict( GameMove mine, GameMove theirs ) {
	switch( theirs ) {
		case ROCK:
			return PAPER;
		case SCISSORS:
			return ROCK;
		case PAPER:
			return SCISSORS;
		default:
			debug( "We should NEVER get here. Unrecognized move.\r\n");
			return 0;
	}
}

GameMove robin( GameMove mine, GameMove theirs ) {
	return predict( mine, mine );
}
// Only plays rock
void rockPlayer () {
	debug ("Rock player is starting. \r\n");
	char *name = "Dwayne J.";
	
	genericPlayer (name, ROCK, &repeat, 3);
}
// Only plays scissors
void scissorsPlayer () {
	debug ("Scissors player is starting. \r\n");
	char *name = "Edward S.";

	genericPlayer (name, SCISSORS, &repeat, 3);
}

// Only plays paper
void paperPlayer () {
	debug ("Paper player is starting. \r\n");
	char *name = "Paper Mario";
	
	genericPlayer (name, PAPER, &repeat, 3);
}

void clonePlayer () {
	debug ("Clone player is starting. \r\n");
	TID id = MyTid();
	char name[] = "EvilClone  ";
	// copy the id into the name
	itoa( id % 64, &name[9] );

	debug("%s (%d) is starting. \r\n", name, id);

	genericPlayer (name, PAPER, &predict, 3);
}

void robinPlayer () {
	debug ("Round Robin player is starting. \r\n");
	TID id = MyTid();
	char name[] = "RobinHood  ";
	// copy the id into the name
	itoa( id % 64, &name[9] );

	debug("%s (%d) is starting. \r\n", name, id);

	genericPlayer (name, SCISSORS, &robin, 3);
}
