/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#define DEBUG

#include <bwio.h>

#include "debug.h"
#include "error.h"
#include "nameserver.h"


void ns_run () {
	debug ("ns_run: The Name Server is about to start. \r\n");	
	NameServer ns;
	int 	   senderTid;
	NSRequest  req;
	int 	   ret;

	// Initialize the Name Server
	ns_init (&ns);

	FOREVER {
		// Receive a server request
		debug ("Name Server is about to receive. \r\n");
		Receive ( &senderTid, (char *) &req, sizeof(NSRequest) );
		debug ("ns Receive: fromTid=%d name=%s type=%d \r\n", senderTid, req.name, req.type);

		switch (req.type) {
			case REGISTERAS:
				// TODO return error if the name already exists
				ns_insert (&ns, req.name, senderTid);

				assert ( ns_lookup(&ns, req.name) == senderTid );
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

		debug ("Name Server is about to reply to %d. \r\n", senderTid);
		Reply ( senderTid, (char *) &ret, sizeof(int) );
		debug ("Name Server returned from Replying to %d. \r\n", senderTid);

		// TODO: Clear out req? Is this needed?
		req.name[0] = 0;
		req.type = 0;
	}
}

void ns_init (NameServer *ns) {
	debug ("ns_init: name server=%x \r\n", ns);
	ns->ptr = 0;
}

void ns_insert (NameServer *ns, const char *name, int tid) {
	debug ("ns_insert: ns=%x name=%s tid=%d ptr=%d\r\n", ns, name, tid, ns->ptr);
	
	int itr = 0;
	NSEntry e;
	strncpy (e.name, name, NAME_LEN); // Deep copy the string so we have it
	e.tid = tid;
	
	// Determine if this name is already in the server.
 	NSEntry *tmp;
	while ( itr < ns->ptr ) {	
		tmp = &ns->entries[itr];
		if ( strcmp ( tmp->name, name ) == 0 ) {
			debug ("   ns_insert: Resetting tid of %s to %d \r\n", name, tid);
			tmp->tid = tid;
			break;
		}
		itr ++;
	}
	// Add the entry if the name isn't in the server.
	if (itr >= ns->ptr) {
		debug ("   ns_insert: Adding %s, since it is not in. \r\n", name);
		ns->entries[ns->ptr++] = e; 
	}
	assert ( itr < NUM_NS_ENTRIES );
}

int ns_lookup (NameServer *ns, const char *name) {
	debug ("ns_lookup: ns=%x name=%s \r\n", ns, name);
	int itr = ns->ptr;
	
	assert ( itr < NUM_NS_ENTRIES );
	// Linearly traverse through the entries
	while ( --itr >= 0 ) {
		debug ("   %d->{%s, %d} == %s ? %d\r\n", itr, 
			   ns->entries[itr].name, ns->entries[itr].tid,
			   name, strcmp(name, ns->entries[itr].name));
		// If we've found a match,
		if ( strcmp (ns->entries[itr].name, name) == 0 ) {
			// Return the corresponding tid
			return ns->entries[itr].tid;
		}
	}
	
	// Otherwise, return an error
	debug("	  ns_lookup: %s NOT_FOUND \r\n", name);
	return NOT_FOUND;
}
