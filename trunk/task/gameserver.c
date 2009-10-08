/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * A Rock, Paper, Scissors Server
 */

#define DEBUG
#define	MSG_LEN		1024

#include <bwio.h>
#include <ts7200.h>

#include "debug.h"
#include "globals.h"
#include "requests.h"
#include "gameplayer.h"
#include "gameserver.h"

void server_run () {

	int senderTid;
	char msgBuffer[MSG_LEN];
	PlayerRequest req;
	Player tmpPlayer;
	GameServer server;
	ServerReply reply;


	int numNewPlayers = 0;
	
	
	// Initialize the Rock, Paper, Scissors Server
	server_init (&server);

	FOREVER {
		Receive ( &senderTid, (char *) &req, sizeof(PlayerRequest));

		switch(req.type) {
			case SIGNUP:
				numNewPlayers += 1;

				// Create a Player instance
				player_init ( &tmpPlayer, senderTid, req.name );

				// Add to the players queue
				server_addPlayer ( server, tmpPlayer );
				
				// If 2 players, start them playing by replying to each
				// Otherwise, do nothing
				if ( numNewPlayers == 2 ) {
					numNewPlayers = 0;
					
					reply.result = START;
					tmpPlayer = server->players->prevPlayer; // tail

					Reply (tmpPlayer.tid, 

				}
				break;
			
			case PLAY:
				// Store the move
				// If opponent has played, determine the winner
					// Reply to both
				// Else,
					// Wait for opponent to make their move (do nthing)
				break;
			
			
			
			case QUIT:
				// Stop accepting calls from this player
				// On the next PLAY call from their opponent, send that this
				// one quit.
				break;
		
			
			default:
				error (1, "Invalid RPS type.");
				break;
		}
	}
}

void server_init (GameServer *server) {
	server->players = 0;
	server->numPlayers = 0;
}

// Kick me if you want, but I wanted a linked list and 
// do not want to share kernel and user task code.
void addPlayer (GameServer *s, Player *p) {
	assert ( s != 0 );
	assert ( p != 0 );
	
	Player *head = s->players;

	if ( head == 0 ) {
		p->nextPlayer = p;
		p->prevPlayer = p;

		s->players = p;
	}
	else {
		Player *tail = head->prevPlayer;
		assert ( tail != 0 );
		assert ( tail->nextPlayer != 0 );
		assert ( tail->prevPlayer != 0 );

		p->nextPlayer = head;
		p->prevPlayer = tail;

		head->prevPlayer = p;
		tail->nextPlayer = p;
	}

	// Increment the counter
	s->numPlayers += 1;

	assert ( s->players != 0 );
}

void server_removePlayer ( GameServer *s, const char *name ) {
	

	// Reduce the counter
	s->numPlayers -= 1;


}
