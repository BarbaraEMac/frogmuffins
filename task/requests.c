/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include <string.h>
#include "requests.h"
#include "switch.h"

#define SWI(n) asm("swi #" #n)



int Create (int priority, Task code ) {
	SWI(1);
	//return syscall(priority, (int) code, 0, CREATE);
}

TID MyTid () {
	SWI(2);
//	return syscall(0, 0, 0, MYTID);
}

TID MyParentTid() {
	SWI(3);
	//return syscall(0, 0, 0, MYPARENTTID);
}

void Pass () {
	SWI(4);
	//syscall(0, 0, 0, PASS);
}

void Exit () {
	SWI(5);
	//syscall(0, 0, 0, EXIT);
}

// This is what a task calls.
int Send (TID tid, char *msg, size_t msglen, char *reply, size_t rpllen) {
	SWI(6);
}


int Receive (TID *tid, char *msg, size_t msglen) {
	SWI(7);
}


int Reply (TID tid, char *reply, size_t rpllen) {
	SWI(8);
}


/*
 * Returns.
 •  0 – success.
 • -1 – if the nameserver task id inside the wrapper is invalid.
 • -2 – if the nameserver task id inside the wrapper is not the name
    server.
 */
int RegisterAs (char *name) {
	NSRequest req;
	int ret;
	req.type = REGISTERAS;
	strncpy( req.name, name, sizeof(req.name));
	Send( NS_TID, (char*) &req, sizeof(NSRequest), (char*) &ret, sizeof(int) );
	return ret;
}

/*
Returns.
 • tid – the task id of the registered task.
 • -1 – if the nameserver task id inside the wrapper is invalid.
 • -2 – if the nameserver task id inside the wrapper is not the
    nameserver.
*/

TID WhoIs (char *name) {
	NSRequest req;
	TID tid;
	req.type = WHOIS;
	strncpy( req.name, name, sizeof(req.name));
	Send( NS_TID, (char*) &req, sizeof(NSRequest), (char*) &tid, sizeof(TID) );
	return tid;
}
