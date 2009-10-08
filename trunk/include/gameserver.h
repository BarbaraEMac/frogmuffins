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
} RPSReqType;

typedef struct {
	int tid;
	char *name;

	int nextPlayer;
	int prevPlayer;

} Player;

typedef Player *playerQueue;

typedef struct {
	playerQueue players;
} RPSserver;

typedef struct {
	RPSReqType type;
	const char *name;
	char 		move;		// R, P, S
} PlayerRequest;

typedef struct {
	char result;
	char *opponent;
} ServerReply;

void rps_init (RPSserver *s);

void rps_run ();

void rps_signup (Player *p);

