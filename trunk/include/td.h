/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#ifndef __TD_H__
#define __TD_H__

#define NUM_PRIORITY 	3
#define USER_START 		0x00044f88
#define USER_END		0x01fdd000

// Pointer the beginning of task execution
typedef void (* Task) ();

enum TASK_STATE {
	ACTIVE = 0,		 // Only 1 task will ever be active
	READY,				// Task may be selected to be active
	BLOCKED,			// Task is waiting for something
	DEFUNCT			 // Task will never run again :(
} taskState;

typedef struct taskdesc TD;
typedef TD* Queue;

struct taskdesc {
	int spsr;			// Saved Processor State Register
	int *sp;			// Stack Pointer
	int returnValue;	// Value to pass to asm if we need to return anything to a syscall
	
	int id;			 	// A unique identifying id
	int parentId;		// The unique id of the parent

	// TODO: Do not store the priority of the task?
	// Yes, because the task might ask for it.
	int priority;		// A priority value (ranges from 0->4)
						
	enum TASK_STATE state;	// State of the task

	TD *nextPQ; // Link to the next TD in the PQ
	TD *prevPQ; // Link to the prev TD in the PQ
};

typedef struct {
	
	TD tdArray[64];

	int frontPtr;	// Not currently used
	int backPtr;	

    Queue ready[NUM_PRIORITY];
	int highestPriority;

	int nextId;		// TODO: Should be the same as backPtr (off by 1)

	Queue blocked;

	Queue defunct;	// Not currently used

} PQ;

//TODO: Comments
void pq_init (PQ *pq);
TD * td_init (int priority, Task start, int parentId, PQ *pq);

void pq_insert (PQ *pq, TD *td);
TD *pq_popReady (PQ *pq);

void queue_push (Queue *head, TD *newTail);		// Returns the new head of the Queue
TD * queue_pop  (Queue *head);

TD * td_create (int priority, Task start, int parentId, PQ *pq);

#endif
