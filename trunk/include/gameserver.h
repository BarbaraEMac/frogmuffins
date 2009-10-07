/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * A Rock, Paper, Scissors Server
 */


typedef enum {
	SIGNUP = 0,
	PLAY,
	QUIT
} Type;

typedef struct {
	int tid;
	char *name;

	int nextPlayer;
	int prevPlayer;

} Player;

typedef Player *Queue;

typedef struct {
	Queue players;
} RPSserver;

typedef struct {
	Type type;
	const char *name;
	char 		move;		// R, P, S
} PlayerRequest;

void rps_init (RPSserver *s);

void rps_run ();

void rps_signup (Player *p);

