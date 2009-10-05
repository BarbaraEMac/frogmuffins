/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#ifndef __TD_H__
#define __TD_H__

#define NUM_PRIORITY 	3	// For now, we will use 3 priorities [0,2]
#define USER_START 		0x00044f88
#define USER_END		0x01fdd000

// Pointer the beginning of task execution
typedef void (* Task) ();

enum TASK_STATE {
	ACTIVE = 0,		 	// Only 1 task will ever be active
	READY,				// Task may be selected to be active
	SEND_BLCK,			// Task is blocked on a send queue
	RCV_BLCK,			// Task is blocked on a receive queue
	RPLY_BLCK,			// Task is blocked on a reply queue
	DEFUNCT			 	// Task will never run again :(
} taskState;

// We want to call task descriptors TDs for short.
typedef struct taskdesc TD;

// At some points, a TD* is really a queue.
typedef TD* Queue;

// TIDs are really ints.
typedef int TID;

/**
 * A Task Descriptor that the kernel uses to define a user task.
 */
struct taskdesc {
	int spsr;			// Saved Processor State Register
	int *sp;			// Stack Pointer
	int returnValue;	// Value to pass to asm if we need to 
						// return anything to a syscall
	TID id;			 	// A unique identifying id
	TID parentId;		// The unique id of the parent

	int priority;		// A priority value (ranges from 0->2)
						
	enum TASK_STATE state;	// State of the task - see enum above

	TD *nextPQ; 		// Link to the next TD in the PQ
	TD *prevPQ; 		// Link to the prev TD in the PQ

	TD *sendQ;			// A circularly linked list of ????

	TD *replyQ;			// A circularly linked list of ????
};

/**
 * The task descriptor priority queues.
 */
typedef struct {
	
	TD tdArray[64];		// Use this until we have dynamic memory management

	int frontPtr;		// TODO: Not currently used
	int backPtr;		// Points to the next unused TD in the array
			
    Queue ready[NUM_PRIORITY]; 	// The ready queue
	int highestPriority;		// The highest non-empty bucket in the ready Q

	TID nextId;			// TODO: Should be the same as backPtr (off by 1)

	Queue blocked;		// A single queue of blocked tasks

	Queue defunct;		// TODO: Not currently used

} PQ;

/**
 * Initialize the priority queues and TDs in the td array.
 * pq - The PQ to init.
 */
void pq_init (PQ *pq);

/**
 * Create a new TD.
 * priority - The priority for the new TD.
 * start - The user task function.
 * parentId - The id of the parent who created this task.
 * pq - The priority queue manager.
 * RETURN: A pointer to the new TD.
 */
TD * td_create (int priority, Task start, TID parentId, PQ *pq);

/**
 * Initialize a single td.
 * priority - The priority for the new TD.
 * start - The user task function.
 * parentId - The id of the parent who created this task.
 * pq - The priority queue manager.
 * RETURN: A pointer to the new TD.
 */
TD * td_init (int priority, Task start, TID parentId, PQ *pq);

/**
 * Inserts the TD into an appropriate priority queue.
 * pq - The priority queue manager.
 * td - The task descriptor to insert.
 */
void pq_insert (PQ *pq, TD *td);

/**
 * Pops the highest priority TD off of the ready queue.
 * pq - The priority queue manager.
 * RETURN: The highest priority READY TD.
 */
TD *pq_popReady (PQ *pq);

/**
 * Given a pointer to the head of a queue, push a new entry as the tail.
 * head - A pointer to the head of the queue.
 * newTail - The TD to insert onto the queue.
 */
void queue_push (Queue *head, TD *newTail);

/**
 * Given a pointer to the head of a queue, pop the head off it.
 * RETURN: The newly popped head TD.
 */
TD * queue_pop  (Queue *head);
#endif