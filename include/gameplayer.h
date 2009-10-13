/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * A Rock, Paper, Scissors Player
 */

#ifndef __GAME_PLAYER_H__
#define __GAME_PLAYER_H__

#include "gameserver.h"


/**
 * The Game Server's Reply to a player will have this form.
 */
typedef struct {
	GameResult 	result;			// Result of the game
	Player 		opponent;		// The player's opponent's name
} ServerReply;

typedef GameMove (* NextMove)( GameMove mine, GameMove theirs );

void genericPlayer (char *name, GameMove start, NextMove getNext, int timesToPlay);

void rockPlayer();

void paperPlayer();

void scissorsPlayer();

void clonePlayer();

#endif
