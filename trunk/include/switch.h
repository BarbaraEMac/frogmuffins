/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#ifndef __SWITCH_H__
#define __SWITCH_H__

#define PC_OFFSET 	14

#include "td.h"
#include "requests.h"
// kernelExit switches from the kernel to the task described, 
// doing the following:
// * save the kernel context
// * change to system state;
// * install the sp the active task
// * pop the registers of the active task from its stack;
// * put the return value in r0;
// * return to svc state;
// * install the spsr of the active task; and
// * install the pc of the active task.
void kernelExit(TD *active, Request *req);

// kernelEntry allows a task to return exectution to the kernel, 
// doing the following:
// * acquire the arguments of the request
// * acquire the lr, which is the pc of the active task;
// * change to system mode;
// * overwrite lr
// * push the registers of the active task onto its stack;
// * acquire the sp of the active task;
// * return to svc state;
// * acquire the spsr of the active task;
// * pop the registers of the kernel from its stack;
// * fill in the request with its arguments;
// * put the sp and the spsr into the TD of the active task.
void kernelEnter();

// handles hardware interrupts by jumping into kernel in a simiral way the 
// kernelEntry does
void interruptHandler();

// This is not currently used.
// We might change this in the future if needed.
//int syscall(int arg0, int arg1, int arg3, enum RequestCode type);

#endif
