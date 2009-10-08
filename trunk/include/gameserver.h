/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * A Rock, Paper, Scissors Server
 */

#ifndef __GAME_SERVER_H__
#define __GAME_SERVER_H__

#define NUM_MATCHES			64

#include "globals.h"

/**
 * Keeps track of a player instance who has registered with the game server.
 */
typedef struct {
	TID tid;		// The task id of the player
	char *name;		// The name of the player
	GameMove move;	// The player's move
} Player;

typedef struct {
	Player *a;
	Player *b;
	int moves;
} MatchUp;

/**
 * A Game Server
 */
typedef struct {
	MatchUp matches[NUM_MATCHES];
	int ptr;
} GameServer;


void gameserver_init (GameServer *s);

void gameserver_run ();

void gameserver_addPlayer (GameServer *s, Player *p);

MatchUp *gameserver_findMatchUp (GameServer *s, TID tid);

void match_init (MatchUp *m);

void match_getPlayers (MatchUp *m, TID tid, Player *a, Player *b);

int match_play (MatchUp *m);
#endif
