/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

// These are really system calls - Cowan calls them "requests"

#ifndef __REQUESTS_H__
#define __REQUESTS_H__

#include "globals.h"

#define NS_TID 		1
#define NAME_LEN	12

enum NSRequestCode {
	REGISTERAS = 1,
	WHOIS
};

typedef struct {
	enum NSRequestCode type;
	char name[NAME_LEN];
} NSRequest;

// More to be added later
enum RequestCode {
    CREATE = 1,
    MYTID,
    MYPARENTTID,
	SEND,
	RECEIVE,
	REPLY,
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
int Create (int priority, Task code);

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

#endif
