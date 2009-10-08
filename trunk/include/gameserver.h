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
	int tid;		// The task id of the player
	char *name;		// The name of the player
	
	int nextPlayer;	// Pointer to the next player in the circular queue
	int prevPlayer;	// Pointer to the previous player in the circular queue

} Player;

typedef Player *playerQueue;

/**
 * A Game Server
 */
typedef struct {
	playerQueue players; // A queue of the players as they have signed up
} GameServer;




void rps_init (GameServer *s);

void rps_run ();

void rps_signup (Player *p);

#endif
