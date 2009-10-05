/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include "requests.h"
#include "switch.h"

#define SWI(n) asm("swi #" #n)

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


int RegisterAs (char *name) {
	SWI(9);
}


int WhoIs (char *name) {
	SWI(10);
}
