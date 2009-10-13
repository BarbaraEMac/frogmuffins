/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#ifndef __TD_H__
#define __TD_H__

#include "globals.h"
#include "requests.h"

#define NUM_PRIORITY 	3	// For now, we will use 3 priorities [0,2]
#define USER_START 		0x00044f88
#define USER_END		0x01fdd000
#define NUM_TDS	 		64
#define NUM_BITFIELD	NUM_TDS/32
#define	STACK_SIZE		0x40000
#define STACK_BASE		0x260000

enum TASK_STATE {
	ACTIVE = 0,		 	// Only 1 task will ever be active
	READY,				// Task may be selected to be active
	SEND_BLKD,			// Task is blocked on a send queue
	RCV_BLKD,			// Task is blocked on a receive queue
	RPLY_BLKD,			// Task is blocked waiting for a reply - It is not on a queue
	DEFUNCT			 	// Task will never run again :(
} taskState;

// We want to call task descriptors TDs for short.
typedef struct taskdesc TD;

// At some points, a TD* is really a queue.
typedef TD* Queue;

// Bitfield is also just an int
typedef int BitField;
/**
 * A Task Descriptor that the kernel uses to define a user task.
 */
struct taskdesc {
	volatile int spsr;				// Saved Processor State Register
	union {
		int volatile * volatile sp;		// Stack Pointer
		ReqArgs * volatile a;			// request arguments in a neatly avaiable union
	};
	int returnValue;		// Value to pass to asm if we need to 
							// return anything to a syscall
	int *sb;				// Stack base
	TID id;			 		// A unique identifying id
	TID parentId;			// The unique id of the parent

	int priority;			// A priority value (ranges from 0->2)
						
	enum TASK_STATE state;	// State of the task - see enum above

	TD *nextPQ; 			// Link to the next TD in the PQ
	TD *prevPQ; 			// Link to the prev TD in the PQ

	Queue sendQ;			// A circularly linked list of TDs that 
							// have called send() this TD a message
};

/**
 * The task descriptor priority queues.
 */
typedef struct {
	
	TD tdArray[NUM_TDS]; 		// Stores all the TDs

    Queue ready[NUM_PRIORITY]; 	// The ready queue
	int highestPriority;		// The highest non-empty bucket in the ready Q

	TID lastId;					// Last id to used

	Queue blocked;				// A single queue of blocked tasks

	BitField empty[NUM_BITFIELD];// bitfield mask telling us which td's are used

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
 * Fetches a TD from the task id.
 * pq - The priority queue manager.
 * tid - The id of the task to find.
 * RETURN: A pointer to the TD corresponding to the given tid.
 */
TD *pq_fetchById (PQ *pq, TID tid);

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
