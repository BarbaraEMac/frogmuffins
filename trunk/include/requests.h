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
#define FRAME_SIZE 	23

// KERNEL SPECIFIC CONSTANTS
#define NS_TID 		1
#define NAME_LEN	12

typedef char TaskName[NAME_LEN];

// A Driver function needs this signature.
typedef int (* Driver) (char * event, size_t eventlen);

// User tasks can AwaitEvent() on these.
// ie. These are the eventId's.
enum INTERRUPTS {
	UART1 = 23,	
	UART2 = 25,
	TIMER1 = 4,
	TIMER2 = 5
} interruptTypes;

// User tasks can request the following things from a Name Server:
enum NSRequestCode {
	REGISTERAS = 1,
	WHOIS,
};

// User tasks can request the following things from a Clock Server:
enum CSRequestCode {
	DELAY = 1,
	TIME,
	DELAYUNTIL,
	NOTIFY
};

// Any request made to the NameServer needs to be of this form.
typedef struct {
	enum NSRequestCode type;
	TaskName name;
} NSRequest;

// Any request made to the ClockServer needs to be of this form.
typedef struct {
	enum CSRequestCode type;
	int  ticks;
} CSRequest;

// NOTE: These must match with the SWI() in requests.c.
enum RequestCode {
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
	HARDWAREINT = 99
};

// A Neat holder that lets us reference arguments based on the syscall.
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
		int dummy[FRAME_SIZE - 4]; // this is a filler since we use the
								 // arguments as they are passed in to the
								 // system request
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
		int eventId;
		char *event;
		size_t eventLen;
	} awaitEvent;
	struct {
		int eventId;
		Driver driver;
	} installDriver;
} ReqArgs;

typedef struct {
	enum RequestCode volatile type; // The type of the request 
									// Includes the number of arguments.
	ReqArgs * volatile a;			// Place in user memory where arguments are stored.
	
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
int AwaitEvent (int eventid, char *event, size_t eventlen);

/**
 * Install a driver to run to handle an interrupt from hardware
 * This driver gets run in priviledged mode immediately after an
 * interrupt occurs, and MUST turn off the source of the interrupt
 */
int InstallDriver (int eventid, Driver driver);

/** 
 * Wait for the given time.
 */
int Delay (int ticks, TID csTid);

/**
 * Return the time since the clock server started.
 */
int Time (TID csTid);

/**
 * Wait until the given time.
 */
int DelayUntil (int ticks, TID csTid);

/**
 * Get a character from the given channel.
 * channel - COM1 for train controller; COM2 for keyboard/monitor
 */
int Getc (int channel);

/**
 * Send the given character over the specified UART.
 * channel - COM1 for train controller; COM2 for keyboard/monitor
 * ch - The character to send
 */
int Putc (int channel, char ch);

#endif
