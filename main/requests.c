/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include "requests.h"
#include "switch.h"


int Create (int priority, void (*code) () ) {
	asm("swi #1");
	//return syscall(priority, (int) code, 0, CREATE);
}

int MyTid () {
	asm("swi #2");
//	return syscall(0, 0, 0, MYTID);
}

int MyParentTid() {
	asm("swi #3");
	//return syscall(0, 0, 0, MYPARENTTID);
}

void Pass () {
	asm("swi #4");
	//syscall(0, 0, 0, PASS);
}

void Exit () {
	asm("swi #5");
	//syscall(0, 0, 0, EXIT);
}

int Test5( int a0, int a1, int a2, int a3, int a5) {
	asm("swi #0x42");
}

