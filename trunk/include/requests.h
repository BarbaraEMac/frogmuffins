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

// A Neat holder that lets us reference arguments based on the syscall
typedef union {
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
	enum RequestCode type; 	// The type of the request 
							// Includes the number of arguments.
	ReqArgs *a;				// Place in user memory where arguments are stored.
	
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

#endif
