/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __SERVERS_H__
#define __SERVERS_H__

#define CLOCK_NAME 		"ClockServer"
#define SERIAL_IO_NAME  "IOServer"

#define NUM_ENTRIES		32
#define	ENTRY_LEN		80
#define NUM_NS_ENTRIES	256

#include "globals.h"
#include "requests.h"

enum SvrRequest {
	REGISTERAS = 1,
	WHOIS,
	DELAY,
	TIME,
	DELAYUNTIL,
	NOTIFY,
	GETC,
	PUTC,
	GETSTR,
	PUTSTR
};

// Any request made to the NameServer needs to be of this form.
typedef struct {
	enum SvrRequest type;
	TaskName		name;
} NSRequest;

// Any request made to the ClockServer needs to be of this form.
typedef struct {
	enum SvrRequest type;
	int  			ticks;
} CSRequest;

// Any request made to the SerialIO Server has this form.
typedef struct {
	enum SvrRequest type;
	int				channel;
	char 		    data[ENTRY_LEN];
} IORequest;

//-----------------------------------------------------------------------------
//--------------------------------Name Server----------------------------------
//-----------------------------------------------------------------------------
/**
 * A Name Server Entry
 * Stores a task name and task id.
 */
typedef struct {
	char name[NAME_LEN];	// The name of the task
	TID  tid;				// The task id
} NSEntry;

/**
 * A Name Server
 * Stores a mapping between task id and their name.
 * User tasks can register with the NS to give a name.
 * User tasks can request a task id from the NS given a name.
 */
typedef struct {
	NSEntry entries[NUM_NS_ENTRIES]; // The entries for the Name Server
	int 	nextEmpty;				 // A pointer to the next empty NS entry
} NameServer;

/**
 * Initialize the nameserver
 * ns - The name server that user tasks can register with.
 */
void ns_init (NameServer *ns);

/**
 * The main function for the Name Server task.
 */
void ns_run ();

/**
 * Insert a {name, tid} entry into the name server database.
 * ns - The name server that user tasks can register with.
 * name - The name for the tid
 * tid - The tid for the given name
 */
void ns_insert (NameServer *ns, const char *name, int tid);

/**
 * Returns the tid of the task that registered with the given name
 * ns - The name server that user tasks can register with.
 * name - The name for the tid
 * RETURN: The corresponding tid
 */
NSEntry * ns_lookup (NameServer *ns, const char *name);

//-----------------------------------------------------------------------------
//-------------------------------Clock Server----------------------------------
//-----------------------------------------------------------------------------

/**
 * Any task that is sleeping on the clockserver will use this structure.
 */
typedef struct sleeper Sleeper;
struct sleeper {
	TID tid;		// The id of the sleeping task
	int endTime;	// The wake up time

	Sleeper *next;	// Pointer to next sleeper in the queue
	Sleeper *prev;	// Pointer to the previous sleeper in the queue
};

/**
 * All of the Clock Server work is done in here.
 */
void cs_run ();

/**
 * Initialize the clock server.
 */
int cs_init (Sleeper **sleepersQ);

// List operators
void list_insert ( Sleeper **head, Sleeper *toAdd );

void list_remove ( Sleeper **head, Sleeper *toAdd );

// Notifier
/**
 * All of the Clock Server's notifier's work is done in here.
 */
void notifier_run();

/**
 * Initialize the Clock Server's Notifier.
 */
void notifier_init();

//-----------------------------------------------------------------------------
//--------------------------------Serial IO------------------------------------
//-----------------------------------------------------------------------------
typedef struct {
	char sendBuffer[NUM_ENTRIES][ENTRY_LEN];
	char recvBuffer[NUM_ENTRIES][ENTRY_LEN];
	

	TD *queue;
} SerialIOServer;

/**
 * All of the Serial IO work is done in here.
 */
void ios_run ();

/**
 * Initialize the Serial IO Server structure.
 */
void ios_init (SerialIOServer *server);

#endif
