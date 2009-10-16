/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
//#define DEBUG

#include <bwio.h>

#include "debug.h"
#include "error.h"
#include "nameserver.h"

void ns_run () {
	debug ("ns_run: The Name Server is about to start. \r\n");	
	NameServer ns;
	int 	   senderTid;
	NSRequest  req;
	int 	   ret, len;

	// Initialize the Name Server
	ns_init (&ns);

	FOREVER {
		// Receive a server request
		debug ("ns: is about to Receive. \r\n");
		len = Receive ( &senderTid, (char *) &req, sizeof(NSRequest) );
		debug ("ns: Received: fromTid=%d name='%s' type=%d  len=%d\r\n", senderTid, req.name, req.type, len);
	
		assert( len == sizeof(NSRequest) );

		switch (req.type) {
			case REGISTERAS:
				ns_insert (&ns, req.name, senderTid);

				assert ( ns_lookup(&ns, req.name)->tid == senderTid );
				break;
			case WHOIS:
				ret = ns_lookup(&ns, req.name)->tid;
				break;
			default:
				ret = NS_INVALID_REQ_TYPE;
				debug (1, "Nameserver request type is not valid.");
				break;
		}

		debug ("ns: about to Reply to %d. \r\n", senderTid);
		Reply ( senderTid, (char *) &ret, sizeof(int) );
		debug ("ns: returned from Replying to %d. \r\n", senderTid);

		// No need to clear out req, it's completely overwritten by Receive
	}
}

void ns_init (NameServer *ns) {
	debug ("ns_init: name server=%x \r\n", ns);
	ns->nextEmpty = 0;

	assert ( MyTid() == NS_TID );
}

void ns_insert (NameServer *ns, const char *name, int tid) {
	debug ("ns_insert: ns=%x name='%s' tid=%d nextEmpty=%d\r\n", ns, name, tid, ns->nextEmpty);
	
	NSEntry *e;
	assert ( ns->nextEmpty < NUM_NS_ENTRIES );
	
	// Determine if this name is already in the server.
	e = ns_lookup( ns, name );

	if( (int) e == NOT_FOUND ) { 	// Add the entry after last entry.
		debug ("\tns_insert: Adding %s, with tid: . \r\n", name, tid);
		// Get the entry after the last one
		e = &ns->entries[ns->nextEmpty++]; 
		// Deep copy the name
		strncpy (e->name, name, NAME_LEN);
	} else {						// A match was found
		debug ("\tns_insert: Overwriting tid of %s to %d \r\n", name, tid);
	}

	// Update the tid
	e->tid = tid;
}

NSEntry * ns_lookup (NameServer *ns, const char *name) {
	debug ("ns_lookup: ns=%x name='%s' \r\n", ns, name);
	int i;
	
	// Linearly traverse through the entries
	for ( i = 0; i < ns->nextEmpty; i++ ) {
		debug ("\t%d->{%s, %d} == %s ? %d\r\n", i, 
			   ns->entries[i].name, ns->entries[i].tid,
			   name, strcmp(name, ns->entries[i].name));
		// If we've found a match,
		if ( strcmp (ns->entries[i].name, name) == 0 ) {
			// Return the matching NSEntry
			return &ns->entries[i];
		}
	}
	
	// Otherwise, return an error
	debug("\tns_lookup: %s NOT_FOUND \r\n", name);
	return (NSEntry *) NOT_FOUND;
}
