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

void rps_run () {

	int senderTid;
	char msgBuffer[MSG_LEN];
	PlayerRequest req;
	Player tmpPlayer;
	GameServer server;
	
	
	// Initialize the Rock, Paper, Scissors Server
	rps_init (&server);

	FOREVER {
		Receive ( &senderTid, (char *) &req, sizeof(PlayerRequest));

		switch(req.type) {
			case SIGNUP:
				// Add to the players queue
				// If 2 players, start them playing by replying to each
				// Otherwise, do nothing

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

void rps_init (GameServer *server) {
	server->players = 0;
}


