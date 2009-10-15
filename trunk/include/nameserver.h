/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __NAME_SERVER_H__
#define __NAME_SERVER_H__

#include "requests.h"

#define NUM_NS_ENTRIES	256

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
	int 	nextEmpty;			// A pointer to the next empty NS entry
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

#endif
