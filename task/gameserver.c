/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * A Rock, Paper, Scissors Server
 */

//#define DEBUG

#include <bwio.h>
#include <ts7200.h>
#include <string.h>

#include "debug.h"
#include "requests.h"
#include "gameplayer.h"
#include "gameserver.h"

typedef union {
	char name[NAME_LEN];
} nem;


void gs_run () {
	debug ("Game Server is starting up. \r\n");

	int 			senderTid, len;
	MatchUp 	   *match;
	Player 		   *player, *opponent;

	PlayerRequest   req;
	ServerReply 	reply;
	GameServer 		server;
	
	// Initialize the Rock, Paper, Scissors Server
	gs_init (&server);
	assert ( WhoIs ("GameServer") == MyTid() );


	FOREVER {
		// Receive a message from a player!
		debug ("game server: Calling Receive. \r\n");
		len = Receive ( &senderTid, (char *) &req, sizeof(PlayerRequest));
		assert( len = sizeof(PlayerRequest) );
		debug ("game server: Returned from Receive. reqtype=%d\r\n", req.type);

//		assert ( WhoIs(req.name) == senderTid );


		switch(req.type) {
			case SIGNUP:
				// Create a Player instance
				player = &req;
				player->tid  = senderTid;			// NOTE: this overwrites req.typ

				// Add to a match
				match = gs_addPlayer ( &server, player );

				// If 2 players, start them playing by replying to each
				if ( match != 0 ) {
					debug ("game server: We have a match up!\r\n");
					
					assert ( match->a != match->b );
					assert ( match->a->tid != match->b->tid );
					assert ( strcmp(match->a->name, match->b->name) != 0 );

					// Create the reply
					reply.result   = START;

					// Send the replies
					reply.opponent = *match->b;
					debug("game server: Replying to player A %d \r\n", match->a->tid);
					Reply (match->a->tid, (char *)&reply, sizeof(ServerReply));
					reply.opponent = *match->a;
					debug("game server: Replying to player B %d \r\n", match->b->tid);
					Reply (match->b->tid, (char *)&reply, sizeof(ServerReply));
				}
				// Otherwise, do nothing
				break;
			
			case PLAY:
				// Store the move
				match = gs_findMatchUp (&server, senderTid);
				
				// TODO: What is match is 0? We should error
				match->moves++;
				match_getPlayers (match, senderTid, &player, &opponent);
				player->move = req.move;

				// If both players have played, determine a winner.
				if ( match->moves == 2 ) {
					reply.opponent = *opponent;
					reply.result = match_play( player, opponent );
					Reply (player->tid, (char*)&reply, sizeof(ServerReply));

					reply.opponent = *player;
					reply.result = match_play( opponent, player );
					Reply (opponent->tid, (char*)&reply, sizeof(ServerReply));
				} else {
					// Wait for opponent to make their move (do nothing)
				}
				break;
			
			case QUIT:
				// Locate the match
				match = gs_findMatchUp (&server, senderTid);
				
				// Remove the player from the match.
				match_getPlayers (match, senderTid, &player, &opponent);
				if (match->a == player) match->a = 0;
				else match->b = 0;

				// Reply to the player.
				reply.result = YOU_QUIT;
				reply.opponent = *opponent;
				Reply (player->tid, (char*)&reply, sizeof(ServerReply));
				
				// Increment the total number of moves for this match.
				match->moves += 1;
				break;

			default:
				error (1, "Invalid RPS type.");
				Exit();
				break;
		}
	}
}

void gs_init (GameServer *server) {
	debug ("gs_init: s: %x \r\n", server);

	// Register with the Name Server
	RegisterAs ("GameServer");
	debug ("    GAME SERVER HAS REGISTERED. \r\n");
	
	server->matchCount = 0;
	
	int i;
	for ( i = 0; i < NUM_MATCHES; i ++ ) {
		match_init ( &server->matches[i] );
	}
}

MatchUp *gs_addPlayer (GameServer *s, Player *p) {
	debug ("gs_addPlayer: server: %x player: '%s'\r\n", s, p->name);
	assert ( s->playerCount < NUM_MATCHES*2 );

	MatchUp *m = &s->matches[s->matchCount];
	s->players[s->playerCount] = *p;	// this deep copies the name by MAGIC
	p = &s->players[s->playerCount++];
	
	if ( m->a == 0 ) {
		debug("\tgs_AddPlayer: Adding as A\r\n");
		m->a = p;
		return 0;
	} else {
		debug("\tgs_AddPlayer: Adding as B\r\n");
		m->b = p;
		s->matchCount++;
		return m;
	}
}

MatchUp *gs_findMatchUp (GameServer *s, TID tid) {
	debug ("gs_findMatchUp: server: %x tid: (%d)\r\n", s, tid);
	int i;
	MatchUp *m;
	
	for ( i = 0; i < s->matchCount ; i ++ ) {
		m = &s->matches[i];

		if ( (m->a->tid == tid) || (m->b->tid == tid) ) {
			debug ("\tgs_findMatchUp: found A='%s' B='%s'\r\n", m->a->name, m->b->name);
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

void match_getPlayers (MatchUp *m, TID tid, Player **p, Player **o) {
	debug("match_getPlayers: m=%x, tid=%d \r\n", m, tid);
	*p = ( m->a->tid == tid ) ? m->a : m->b;
	*o = ( m->a->tid != tid ) ? m->a : m->b;
}

GameResult match_play (Player *player, Player *opponent) {
	debug ("match_play: player='%s' opponent='%s'\r\n", player->name, opponent->name);
	// If an opponent quit, let the player know.
	if ( opponent == 0 ) {
		return OPP_QUIT;
	}
	
	// Determine a winner!
	GameMove aMove = player->move;
	GameMove bMove = player->move;
	
	// Reset the moves.
	player->move = 0;
	opponent->move = 0;

	if ( aMove == bMove ) {
		return TIE;
	} else if (	(aMove == ROCK     && bMove == SCISSORS) || 
			  	(aMove == PAPER    && bMove == ROCK)     || 
			  	(aMove == SCISSORS && bMove == PAPER) ) {
		return  LOSE;
	} else {
		return WIN;
	}
}
