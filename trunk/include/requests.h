/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

// These are really system calls - Cowan calls them "requests"

#ifndef __REQUESTS_H__
#define __REQUESTS_H__

#include <string.h>
#include "globals.h"

#define NS_TID 		1
#define CS_TID 		2 // TODO: Remove this since Bill Cowan said it was WRONG in last term's newsgroup
#define NAME_LEN	12

typedef char TaskName[NAME_LEN];

enum NSRequestCode {
	REGISTERAS = 1,
	WHOIS,
	};

enum CSRequestCode {
	DELAY = 1,
	TIME,
	DELAYUNTIL,
	NOTIFY
};

typedef struct {
	enum NSRequestCode type;
	TaskName name;
} NSRequest;

typedef struct {
	enum CSRequestCode type;
	int  ticks;
} CSRequest;

// More to be added later
enum RequestCode {
    CREATE = 1,
    MYTID = 2,
    MYPARENTTID = 3,
    PASS = 4,
    EXIT = 5,
	SEND = 6,
	RECEIVE = 7,
	REPLY = 8,
	AWAITEVENT = 9
};

// A Neat holder that lets us reference arguments based on the syscall
typedef volatile const union {
	struct {
		int priority;
		Task code;
	} create;
	struct {
		TID tid;
		char *msg;
		size_t msglen;
		char *reply;
		int dummy[18]; // this is a filler since we use the previous stack addr
		size_t rpllen;
	} send;
	struct {
		TID *tid;
		char *msg;
		size_t msglen;
	} receive;
	struct {
		TID tid;
		char *reply;
		size_t rpllen;
	} reply;
	struct {
		char *name;
	} registerAs;
	struct {
		char *name;
	} whoIs;
} ReqArgs;

typedef struct {
	enum RequestCode volatile type; 	// The type of the request 
							// Includes the number of arguments.
	ReqArgs * volatile a;				// Place in user memory where arguments are stored.
	
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
TID MyTid ();

/**
 * Return the task id of the parent task.
 */
TID MyParentTid();

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
int Send (TID tid, char *msg, size_t msglen, char *reply, size_t rpllen);

/**
 * Receive a message.
 */
int Receive (TID *tid, char *msg, size_t msglen);

/**
 * Reply to a message.
 */
int Reply (TID tid, char *reply, size_t rpllen);

/**
 * Register with the nameserver.
 */
int RegisterAs (char *name);

/**
 * Query the name server.
 */
TID WhoIs (char *name);

/**
 * Blocks until the specified event occurs and returns.
 */
int AwaitEvent (int eventid, char *event, int eventlen);

/** 
 * Wait for the given time.
 */
int Delay (int ticks);

/**
 * Return the time since the clock server started.
 */
int Time ();

/**
 * Wait until the given time.
 */
int DelayUntil (int ticks);

#endif
