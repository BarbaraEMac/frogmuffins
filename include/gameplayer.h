/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * A Rock, Paper, Scissors Player
 */

#ifndef __GAME_PLAYER_H__
#define __GAME_PLAYER_H__

/**
 * The result for a single game of RPS.
 */
typedef enum {
	WIN = 0,
	LOSE,
	TIE
} GameResult;

/**
 * The moves a player can make in the game RPS.
 */
typedef enum {
	ROCK = 0,
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
	const char *name;	// Player's name
	RequestType type;	// Request type
	GameMove 	move;	// RPS move
} PlayerRequest;

/**
 * The Game Server's Reply to a player will have this form.
 */
typedef struct {
	GameResult result;	// Result of the game
	char *opponent;		// The player's opponent's name
} ServerReply;






void genericPlayer (char *name, GameMove move, int timesToPlay);

void rockPlayer();

void paperPlayer();

void scissorsPlayer();

#endif
