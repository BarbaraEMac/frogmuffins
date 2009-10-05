/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

// These are really system calls - Cowan calls them "requests"

#ifndef __REQUESTS_H__
#define __REQUESTS_H__

#include "td.h"

// More to be added later
enum RequestCode {
    CREATE = 1,
    MYTID,
    MYPARENTTID,
	SEND,
	RECEIVE,
	REPLY,
	REGISTERAS,
	WHOIS,
    PASS,
    EXIT
};

typedef struct {
	enum RequestCode type; 	// The type of the request 
							// Includes the number of arguments.
	int const * const args;	// Place in user memory where arguments are stored.
	
	// NOTE: The 5th argument is in args[22] 
	// and NOT args[4] as would be expected.
} Request;

//---------------------------------------------------------------------
//---------------------------Task Calls--------------------------------
//---------------------------------------------------------------------

/**
 * Create a new task with given priority and start function
 */
int Create (int priority, Task code );

/**
 * Return the task id
 */
int MyTid ();

/**
 * Return the task id of the parent task.
 */
int MyParentTid();

/**
 * Cease execution, but stay ready to run.
 */
void Pass ();

/**
 * Terminate execution forever.
 */
void Exit ();

/**
 * Send a message to a task.
 */
int Send (int tid, char *msg, int msglen, char *reply, int rpllen);

/**
 * Receive a message.
 */
int Receive (int *tid, char *msg, int msglen);

/**
 * Reply to a message.
 */
int Reply (int tid, char *reply, int rpllen);

/**
 * Register with the nameserver.
 */
int RegisterAs (char *name);

/**
 * Query the name server.
 */
int WhoIs (char *name);

//---------------------------------------------------------------------
//-------------------------Kernel Calls--------------------------------
//---------------------------------------------------------------------

int send (int tid, char *msg, int msglen, char *reply, int rpllen);
int receive (int *tid, char *msg, int msglen);
int reply (int tid, char *reply, int rpllen);
int registerAs (char *name);
int whoIs (char *name);

#endif
