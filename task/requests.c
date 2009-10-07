/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include "requests.h"
#include "switch.h"

#define SWI(n) asm("swi #" #n)

typedef struct {
	enum NSRequestCode type;
	char name[12];
} NSRequest;

// TODO move this function into string.h
typedef int size_t;
/*
 * Copy characters from string
 *
 * Copies the first num characters of source to destination. 
 * If the end of the source C string (which is signaled by a null-character) 
 * is found before num characters have been copied, 
 * destination is padded with zeros until a total of num characters 
 * have been written to it.
 */
char * strncpy ( char *destination, const char * source, size_t num ) {
	int i;
	for ( i=0; (i < num) && source[i]; i++ ) {
		destination[i] = source[i];
	}
	for ( ; i < num; i++ ) {
		destination[i] = '\0';
	}
	return destination;
}


int Create (int priority, Task code ) {
	SWI(1);
	//return syscall(priority, (int) code, 0, CREATE);
}

int MyTid () {
	SWI(2);
//	return syscall(0, 0, 0, MYTID);
}

int MyParentTid() {
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
int Send (int tid, char *msg, int msglen, char *reply, int rpllen) {
	SWI(6);
}


int Receive (int *tid, char *msg, int msglen) {
	SWI(7);
}


int Reply (int tid, char *reply, int rpllen) {
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
	int tid;
	req.type = REGISTERAS;
	strncpy( req.name, name, sizeof(req.name));
	Send( NS_TID, (char*) &req, sizeof(NSRequest), (char*) &tid, sizeof(int) );
	return tid;
}

/*
Returns.
 • tid – the task id of the registered task.
 • -1 – if the nameserver task id inside the wrapper is invalid.
 • -2 – if the nameserver task id inside the wrapper is not the
    nameserver.
*/

int WhoIs (char *name) {
	NSRequest req;
	int tid;
	req.type = WHOIS;
	strncpy( req.name, name, sizeof(req.name));
	Send( NS_TID, (char*) &req, sizeof(NSRequest), (char*) &tid, sizeof(int) );
	return tid;
}
