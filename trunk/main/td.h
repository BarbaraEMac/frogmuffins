/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#ifndef __TD_H__
#define __TD_H__

#define MAX_PRIORITY 2

enum TASK_STATE {
	ACTIVE = 0,		 // Only 1 task will ever be active
	READY,				// Task may be selected to be active
	BLOCKED,			// Task is waiting for something
	DEFUNCT			 // Task will never run again :(
} taskState;

typedef struct taskdesc TD;

struct taskdesc {
	int spsr;			// Saved Processor State Register
	void *sp;			// Stack Pointer
	int returnValue;	// Value to pass to asm if we need to return anything to a syscall
	
	
	void (* start )();	// The first function this runs

	int id;			 	// A unique identifying id
	int parentId;		// The unique id of the parent

	// TODO: Do not store the priority of the task?
	int priority;		// A priority value (ranges from 0->4)
						
	enum TASK_STATE state;	// State of the task

	TD *nextPQ; // Link to the next TD in the PQ
	TD *prevPQ; // Link to the prev TD in the PQ
};

typedef struct {
	
	TD tdArray[64];
	int frontPtr;	// Not currently used
	int backPtr;	

    TD *readyQueue[MAX_PRIORITY + 1];
	int highestPriority;

	int nextId;		// TODO: Should be the same as backPtr (off by 1)

	TD *blockedQueue;

} TDManager;


void firstTaskStart ();

void userTaskStart (TD *t);

TD * getUnusedTask ( TDManager *manager );

int getNextId ( TDManager *manager );

void initTaskDesc ( TD *td, int priority, void (*s)(), int parentId, TDManager *manager );

void insertInReady (TD *td, TDManager *manager);
void insertInBlocked (TD *td, TDManager *manager);
void insertInQueue (TD *head, TD *td);
TD * removeFromQueue (TD *td);

TD * kernCreateTask ( int priority, void (*start)(), int parentId, TDManager *manager );

#endif
