/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * A Rock, Paper, Scissors Server
 */

#ifndef __GAME_SERVER_H__
#define __GAME_SERVER_H__

/**
 * Keeps track of a player instance who has registered with the game server.
 */
typedef struct {
	TID tid;		// The task id of the player
	char *name;		// The name of the player
	
	int nextPlayer;	// Pointer to the next player in the circular queue
	int prevPlayer;	// Pointer to the previous player in the circular queue

} Player;

typedef Player *PlayerQueue;

/**
 * A Game Server
 */
typedef struct {
	PlayerQueue players; // A queue of the players as they have signed up

	int numPlayers;
} GameServer;


void server_init (GameServer *s);

void server_run ();

void server_addPlayer (GameServer *s, Player *p);

void server_removePlayer (GameServer *s, const char *name);

void player_init (Player *p, const char *n, TID tid);
#endif
