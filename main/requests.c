/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include "requests.h"

int Create (int priority, void (*code) () ) {
    asm ("swi #1");
}

int MyTid () {
    asm ("swi #2");
}

int MyParentTid();
    asm ("swi #3");
}

void Pass ();
    asm ("swi #4");
}

void Exit ();
    asm ("swi #5");
}
