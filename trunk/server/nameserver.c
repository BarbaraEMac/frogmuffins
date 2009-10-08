/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include <bwio.h>

#include "debug.h"
#include "error.h"
#include "nameserver.h"


void ns_run () {

	NameServer ns;
	int 	   senderTid;
	NSRequest  req;
	int 	   ret;

	// Initialize the Name Server
	ns_init (&ns);

	FOREVER {
		// Receive a server request
		Receive ( &senderTid, (char *) &req, sizeof(NSRequest) );

		switch (req.type) {
			case REGISTERAS:
				// TODO return error if the name already exists
				ns_insert (&ns, req.name, senderTid);
				break;
			case WHOIS:
				ret = ns_lookup (&ns, req.name);
				break;
			default:
				ret = NS_INVALID_REQ;
				// TODO: do we need this message?
				error (1, "Nameserver request type is not valid.");
				break;
		}

		Reply ( senderTid, (char *) &ret, sizeof(int) );
	}
}

void ns_init (NameServer *ns) {
	ns->ptr = 0;
}

void ns_insert (NameServer *ns, const char *name, int tid) {
	assert ( itr < NUM_NS_ENTRIES );
	
	int itr = 0;
	NSEntry e;
	strncpy (e.name, name, NAME_LEN); // Deep copy the string so we have it
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
