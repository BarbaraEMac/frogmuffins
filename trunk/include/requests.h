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

// ARCHITECTURE SPECIFIC CONSTANTS
#define FRAME_SIZE 	15

// KERNEL SPECIFIC CONSTANTS
#define NS_TID 		2
#define NAME_LEN	12
#define UNUSED_TID	-1

#define NUM_USER_PRIORITIES	10
#define HW_SERVER_PRTY 		1
#define NOTIFIER_PRTY 		HW_SERVER_PRTY - 1
#define OTH_SERVER_PRTY		3
#define OTH_NOTIFIER_PRTY	OTH_SERVER_PRTY - 1
#define	IDLE_PRTY			NUM_USER_PRIORITIES - 1
#define SHELL_PRTY			IDLE_PRTY - 1

#define TRAIN_PRTY			5
#define LOW_SERVER_PRTY		7

typedef char TaskName[NAME_LEN];

// A Driver function needs this signature.
typedef int (* Driver) (char * event, size_t eventlen);

// NOTE: These must match with the SWI() in requests.c.
enum SysCallCode {
    CREATE = 1,
    MYTID = 2,
    MYPARENTTID = 3,
    PASS = 4,
    EXIT = 5,
	SEND = 6,
	RECEIVE = 7,
	REPLY = 8,
	AWAITEVENT = 9,
	INSTALLDRIVER = 10,
	DESTROY = 11,
	HARDWAREINT = 99
};

// A Neat holder that lets us reference arguments based on the syscall.
typedef volatile union {
	const int arg[FRAME_SIZE];
	const struct {
		int priority;
		Task code;
	} create;
	const struct {
		TID tid;
		char *msg;
		size_t msglen;
		char *reply;
		// We need a filler since we use the arguments as 
		// they are passed in to the system request
		int DUMMY[FRAME_SIZE - 4];	
		size_t rpllen;
	} send;
	const struct {
		TID *tid;
		char *msg;
		size_t msglen;
	} receive;
	const struct {
		TID tid;
		char *reply;
		size_t rpllen;
	} reply;
	const struct {
		int eventId;
		char *event;
		size_t eventLen;
	} awaitEvent;
	const struct {
		int eventId;
		Driver driver;
	} installDriver;
	const struct {
		TID tid;
	} destroy;
	int retVal;						// the return value of a syscall
} ReqArgs;

typedef struct {
	enum SysCallCode volatile type; // The type of the request 
									// Includes the number of arguments.
	ReqArgs * volatile a;			// Place in user memory where arguments are stored.
	
} Request;

//---------------------------------------------------------------------
//---------------------------Task Calls--------------------------------
//---------------------------------------------------------------------

/**
 * Create a new task with given priority and start function
 * Return error code on failure, the child's tid otherwise
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
int Send (TID tid, void *msg, size_t msglen, void *reply, size_t rpllen);

/**
 * Receive a message.
 */
int Receive (TID *tid, void *msg, size_t msglen);

/**
 * Reply to a message.
 */
int Reply (TID tid, void *reply, size_t rpllen);

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
int AwaitEvent (int eventid, char *event, size_t eventlen);

/**
 * Install a driver to run to handle an interrupt from hardware
 * This driver gets run in priviledged mode immediately after an
 * interrupt occurs, and MUST turn off the source of the interrupt
 */
int InstallDriver (int eventid, Driver driver);

/*
 * Destroys a  task killing preventing it from running again
 */
int Destroy (TID tid);

/** 
 * Wait for the given time.
 * csTid - The task id of the clock server
 */
int Delay (int ticks, TID csTid);

/**
 * Return the time since the clock server started.
 * csTid - The task id of the clock server
 */
int Time (TID csTid);

/**
 * Wait until the given time.
 * csTid - The task id of the clock server
 */
int DelayUntil (int ticks, TID csTid);

/**
 * Get a character from the given channel.
 * channel - COM1 for train controller; COM2 for keyboard/monitor
 * iosTid - The task id of the serial io server
 */
int Getc (TID iosTid);

/**
 * Send the given character over the specified UART.
 * channel - COM1 for train controller; COM2 for keyboard/monitor
 * ch - The character to send
 * iosTid - The task id of the serial io server
 */
int Putc (char ch, TID iosTid);

/**
 * Sends a string of bytes to a given channel.
 * channel - COM1 for train controller; COM2 for keyboard/monitor
 * str - The bytes to send.
 * strLen - The number of bytes to send.
 * iosTid - The task id of the serial io server
 */
int PutStr (const char *str, int strLen, TID iosTid);


/**
 * Purges the input buffer of the given channel
 */
void Purge (TID iosTid);

/**
 * Similar to getc, but returns TIMEOUT_ERROR if getc does not acquire
 * a character within prescribed amount of time;
 * iosTid - The task id of the serial io server
 * csTid - The task id of the clock server
 * timeout - the number of ticks to wait before sending the timeout err;
 */
int TimeoutGetc( TID iosTid, TID csTid, int timeout );

/**
 * Waits for n tasks to Sync() with this task
 * n - number of Sync() calls to wait for
 */
//void WaitFor ( int n );

/**
 * Sends a Sync() event to the task provided
 * Returns error codes on failure
 */
//int Sync ( TID tid );

#endif
