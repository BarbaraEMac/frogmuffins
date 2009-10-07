/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include <bwio.h>

#include "debug.h"
#include "error.h"
#include "nameserver.h"

#define MSG_LEN 	1024

void ns_run () {

	NameServer ns;
	int 	   senderTid;
	char 	   msgBuffer[MSG_LEN];
	NSRequest  req;
	int 	   ret;
	char 	   retBuf[MSG_LEN];

	// Initialize the Name Server
	ns_init (&ns);

	FOREVER {
		// Receive a server request
		Receive ( &senderTid, msgBuffer, MSG_LEN );

		// Create the request from the string buffer
		memcpy ( (char*)&req, msgBuffer, sizeof (NSRequest) );

		switch (req.type) {
			case REGISTERAS:
				ns_insert (&ns, req.name, senderTid);
				break;
			case WHOIS:
				ret = ns_lookup (&ns, req.name);
				break;
			default:
				// TODO: Invalid request type
				error (1, "Nameserver request type is not valid.");
				break;
		}
		*retBuf = (char) ret;

		Reply ( senderTid, retBuf, MSG_LEN );
	}
}

void ns_init (NameServer *ns) {
	ns->ptr = 0;
}

void ns_insert (NameServer *ns, const char *name, int tid) {
	assert ( itr < NUM_NS_ENTRIES );
	
	int itr = 0;
	NSEntry e;
	memcpy (e.name, name, NAME_LEN); // Deep copy the string so we have it
	e.tid = tid;
	
	// Determine if this name is already in the server.
 	NSEntry *tmp;
	do {
		tmp = &ns->entries[itr];
		if ( strcmp ( tmp->name, name ) == 0 ) {
			tmp->tid = tid;
			break;
		}
	} while ( ++itr != ns->ptr );

	// Add the entry if the name isn't in the server.
	if (itr == ns->ptr) {
		ns->entries[ns->ptr++] = e; 
	}
}

int ns_lookup (NameServer *ns, const char *name) {
	int itr = ns->ptr;
	
	assert ( itr < NUM_NS_ENTRIES );
	// Linearly traverse through the entries
	while ( --itr > 0 ) {
		// If we've found a match,
		if ( strcmp (ns->entries[itr].name, name) == 0 ) {
			// Return the corresponding tid
			return ns->entries[itr].tid;
		}
	}
	
	// Otherwise, return an error
	return NOT_FOUND;
}
