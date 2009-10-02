/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include "requests.h"
#include "switch.h"

#define SWI(n) asm("swi #" #n)


int Create (int priority, void (*code) () ) {
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

