/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * A Rock, Paper, Scissors Server
 */

#define DEBUG

#include <bwio.h>
#include <ts7200.h>

#include "debug.h"
#include "requests.h"
#include "gameplayer.h"
#include "gameserver.h"

void server_run () {

	int 			numNewPlayers = 0;
	int 			senderTid;
	MatchUp 	   *match;
	Player 		   *tmpPlayer;
	Player 		   *opponent;
	PlayerRequest   req;
	ServerReply 	reply;
	GameServer 		server;
	
	// Initialize the Rock, Paper, Scissors Server
	server_init (&server);
	assert ( WhoIs ("GameServer") == MyTid() );

	FOREVER {
		Player newPlayer; // since we can't make this in the switch statement .... C .... :(
		
		// Receive a message from a player!
		Receive ( &senderTid, (char *) &req, sizeof(PlayerRequest));

		assert ( WhoIs(req.name) == senderTid );

		switch(req.type) {
			case SIGNUP:
				// Create a Player instance
				newPlayer.name = req.name;
				newPlayer.tid  = senderTid;
				newPlayer.move = 0;

				// Add to a match
				server_addPlayer ( &server, &newPlayer );
				
				// If 2 players, start them playing by replying to each
				if ( ++numNewPlayers == 2 ) {
					numNewPlayers = 0;
					
					// Get the brand new match
					*match = server.matches[ server.ptr-1 ];

					// Create the reply
					reply.result   = START;
					reply.opponent = match->b->name;

					// Send the replies
					Reply (match->a->tid, (char *)&reply, sizeof(ServerReply));

					reply.opponent = match->a->name;
					Reply (match->b->tid, (char *)&reply, sizeof(ServerReply));
				}
				// Otherwise, do nothing
				break;
			
			case PLAY:
				// Store the move
				match = server_findMatchUp (&server, senderTid);
				
				// TODO: What is match is 0? We should error
				match->moves += 1;
				match_getPlayers (match, senderTid, tmpPlayer, opponent);
				tmpPlayer->move = req.move;

				// If both players have played, determine a winner.
				// TODO: UUUUGGGGGLLLLLYYYYY
				if ( match->moves == 2 ) {
					switch(match_play(match)) {
						case 0 /*TIE*/:
							reply.result = TIE;
							reply.opponent = match->b->name;
							Reply (match->a->tid, (char*)&reply, sizeof(ServerReply));
							reply.opponent = match->a->name;
							Reply (match->b->tid, (char*)&reply, sizeof(ServerReply));
							break;
						
						case 1 /*a won*/:
							reply.result = WIN;
							reply.opponent = match->b->name;
							Reply (match->a->tid, (char*)&reply, sizeof(ServerReply));
							
							reply.result = LOSE;
							reply.opponent = match->a->name;
							Reply (match->b->tid, (char*)&reply, sizeof(ServerReply));
							break;

						case -1 /*b won*/:
							reply.result = WIN;
							reply.opponent = match->a->name;
							Reply (match->b->tid, (char*)&reply, sizeof(ServerReply));
							
							reply.result = LOSE;
							reply.opponent = match->a->name;
							Reply (match->a->tid, (char*)&reply, sizeof(ServerReply));
							
							break;
						case -2 /*Opponent Quit*/:
							reply.result = OPP_QUIT;
							reply.opponent = opponent->name;
							Reply (tmpPlayer->tid, (char*)&reply, sizeof(ServerReply));
							break;
						default:
							assert (1 == 0); // TODO: THis should never happen
							break;
					}
				}
				else {
					// Wait for opponent to make their move (do nthing)
					;
				}
				break;
			
			case QUIT:
				// Locate the match
				match = server_findMatchUp (&server, senderTid);
				
				// Remove the player from the match.
				match_getPlayers (match, senderTid, tmpPlayer, opponent);
				if (match->a == tmpPlayer) match->a = 0;
				else match->b = 0;

				// Reply to the player.
				reply.result = YOU_QUIT;
				reply.opponent = opponent->name;
				Reply (tmpPlayer->tid, (char*)&reply, sizeof(ServerReply));
				
				// Increment the total number of moves for this match.
				match->moves += 1;
				break;

			default:
				error (1, "Invalid RPS type.");
				break;
		}
	}
}

void server_init (GameServer *server) {

	// Register with the Name Server
	RegisterAs ("GameServer");
	
	server->ptr = 0;
	
	int i;
	for ( i = 0; i < NUM_MATCHES; i ++ ) {
		match_init ( &server->matches[i] );
	}
}

void server_addPlayer (GameServer *s, Player *p) {
	MatchUp *m = &s->matches[s->ptr];

	if ( m->a == 0 ) {
		m->a = p;
	}
	else {
		m->b = p;
		s->ptr += 1;
	}
}

MatchUp *server_findMatchUp (GameServer *s, TID tid) {
	int i;
	int totalMatchUpes = s->ptr;
	MatchUp *m;
	
	for ( i = 0; i < totalMatchUpes; i ++ ) {
		m = &s->matches[i];

		if ( (m->a->tid == tid) || (m->b->tid == tid) ) {
			return m;
		}
	}

	return 0;
}

void match_init (MatchUp *m) {
	m->a     = 0;
	m->b     = 0;
	m->moves = 0;
}

void match_getPlayers (MatchUp *m, TID tid, Player *p, Player *o) {
	*p = ( m->a->tid == tid ) ? *m->a : *m->b;
	*o = ( m->a->tid != tid ) ? *m->a : *m->b;
}

int match_play (MatchUp *m) {
	Player *a = m->a;
	Player *b = m->b;
	
	// If an opponent quit, let the player know.
	if ( (a == 0) || (b == 0) ) {
		
		int replyTid;	
		ServerReply reply;
		reply.result = OPP_QUIT;
		
		if ( a == 0 ) {
			replyTid = b->tid;
			reply.opponent = a->name;
		}
		else {
			replyTid = a->tid;
			reply.opponent = b->name;
		}
		
		Reply ( replyTid, (char *)&reply, sizeof(ServerReply) );
		return -2;
	}
	
	// Determine a winner!
	GameMove aMove = a->move;
	GameMove bMove = b->move;
	
	// Reset the moves.
	a->move = 0;
	b->move = 0;

	if ( aMove == bMove ) {
		return 0;
	}
	else if ( (aMove == ROCK     && bMove == SCISSORS)|| 
			  (aMove == PAPER    && bMove == ROCK)    || 
			  (aMove == SCISSORS && bMove == PAPER) ) {
		return -1;
	}
	else {
		return 1;
	}
}
