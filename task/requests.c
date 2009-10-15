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

int RegisterAs (char *name) {
	NSRequest req;
	int ret;
	req.type = REGISTERAS;
	strncpy( req.name, name, sizeof(req.name));
	Send( NS_TID, (char*) &req, sizeof(NSRequest), (char*) &ret, sizeof(int) );
	return ret;
}


TID WhoIs (char *name) {
	NSRequest req;
	TID tid;
	req.type = WHOIS;
	strncpy(req.name, name, sizeof(req.name));
	Send( NS_TID, (char*) &req, sizeof(NSRequest), (char*) &tid, sizeof(TID) );
	return tid;
}

int AwaitEvent (int eventid, char *event, int eventlen) {
	
}

int Delay ( int ticks ) {
	char ret[5];
	CSRequest req;
	req.type = DELAY;
	Send(CS_TID, (char*) &req, sizeof(CSRequest), (char*) ret, sizeof(char)*5);

	return ret;
}

int Time () {
	int time;
	CSRequest req;
	req.type = TIME;
	Send(CS_TID, (char*) &req, sizeof(CSRequest), (char*) &time, sizeof(int));

	return time;
}

int DelayUntil (int ticks) {
	char ret[5];
	CSRequest req;
	req.type = DELAYUNTIL;
	Send(CS_TID, (char*) &req, sizeof(CSRequest), (char*) ret, sizeof(char)*5);
	
	return ret;
}
