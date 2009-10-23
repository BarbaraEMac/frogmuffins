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
#define GAMESERVER_NAME		"GameServer"

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
	ROCK = 'R',
	PAPER = 'P',
	SCISSORS = 'S'
} GameMove;

/**
 * The type of request a player can make to the server.
 */
typedef enum {
	SIGNUP = 0,
	PLAY,
	QUIT = 'Q'
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


/**
 * Initialize the game server.
 */
void gs_init (GameServer *s);

/**
 * All of the work that the Game Server does is in here.
 */
void gs_run ();

/**
 * Add a player to the matches.
 * s - The Game Server
 * p - The new player to add
 * Returns: 
 *		A match if both players have registered.
 *		0 otherwise.
 */
MatchUp *gs_addPlayer (GameServer *s, Player *p);

/**
 * Given a task id, return the match it is playing in.
 * s - The Game Server
 * tid - The task's id
 * Return: The match
 * 		   0 otherwise
 */
MatchUp *gs_findMatchUp (GameServer *s, TID tid);

/**
 * Initialize the input match.
 */
void match_init (MatchUp *m);

/**
 * Given a match, return the two players in it.
 * Returns:
 * a - The player with the matching tid
 * b - The player's opponent
 */
void match_getPlayers (MatchUp *m, TID tid, Player **a, Player **b);

/**
 * Given two players with two moves, determine the result of the game
 * and return it.
 */
GameResult match_play (Player *player, Player *other);

#endif
