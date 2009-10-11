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
#include "requests.h"
/**
 * The result for a single game of RPS.
 */
typedef enum {
	WIN = 0,
	LOSE,
	TIE, 
	START,
	YOU_QUIT,
	OPP_QUIT
} GameResult;

/**
 * The moves a player can make in the game RPS.
 */
typedef enum {
	ROCK = 1,
	PAPER,
	SCISSORS
} GameMove;

/**
 * The type of request a player can make to the server.
 */
typedef enum {
	SIGNUP = 0,
	PLAY,
	QUIT
} RequestType;

/**
 * Any message from a player will have this form.
 */
typedef struct {
	TaskName 	name;			// Player's name
	union {
		RequestType	type;			// Request type
		TID			tid;			// The task id of the player
	};
	GameMove 	move;			// RPS move
} PlayerRequest;

/**
 * Keeps track of a player instance who has registered with the game server.
 */
typedef PlayerRequest Player;

typedef struct {
	union {
		struct {
			Player *a;
			Player *b;
		};
		Player player[2];
	};
	int moves;
} MatchUp;

/**
 * A Game Server
 */
typedef struct {
	Player	players[NUM_MATCHES*2];
	int 	playerCount;
	MatchUp matches[NUM_MATCHES];
	int 	matchCount;
} GameServer;


void gs_init (GameServer *s);

void gs_run ();

MatchUp *gs_addPlayer (GameServer *s, Player *p);

MatchUp *gs_findMatchUp (GameServer *s, TID tid);

void match_init (MatchUp *m);

void match_getPlayers (MatchUp *m, TID tid, Player **a, Player **b);

GameResult match_play (Player *player, Player *other);
#endif
