/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include "requests.h"

// TOD0: Use the ENUM! Do not hardcode these in!


int Create (int priority, void (*code) () ) {
    asm ("swi #1");

    // TODO: return something ie. get something back
}

int MyTid () {
    asm ("swi #2");
}

int MyParentTid() {
    asm ("swi #3");
}

void Pass () {
    asm ("swi #4");
}

void Exit () {
    asm ("swi #5");
}
