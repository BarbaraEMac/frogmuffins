/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#ifndef __TD_H__
#define __TD_H__

enum TASK_STATE {
	ACTIVE = 0,		 // Only 1 task will ever be active
	READY,				// Task may be selected to be active
	BLOCKED,			// Task is waiting for something
	DEFUNCT			 // Task will never run again :(
} taskState;

typedef struct taskdesc TD;

struct taskdesc {
	int returnValue;	// Value to pass to asm if we need to return anything to a syscall
	int spsr;			// Saved Processor State Register
	void *sp;			// Stack Pointer
	
	
	void (* start )();	// The first function this runs

	int id;			 	// A unique identifying id
	int parentId;		// The unique id of the parent

	int priority;		// A priority value (ranges from 0->4)
						
	enum TASK_STATE state;	// State of the task

	TD *nextPQ; // Link to the next TD in the PQ
	TD *prevPQ; // Link to the prev TD in the PQ
};

void userTaskStart (TD *t);

void firstTask ();

#endif
