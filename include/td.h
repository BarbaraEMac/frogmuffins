/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#ifndef __TD_H__
#define __TD_H__

#include "globals.h"
#include "requests.h"

// Hardware specific constants
#define USER_END		0x01fdd000
#define USER_START 		0x00044f88
#define	STACK_SIZE		0x10000
#define STACK_BASE		0x260000
#define	DEFAULT_PSR		0x10

// Kernel specific constants
#define NUM_PRIORITY 	10	// For now, we will use 10 priorities [0,9]
#define NUM_TDS	 		64
#define NUM_BITFIELD	NUM_TDS/32
#define NUM_INTERRUPTS	64

enum TASK_STATE {
	ACTIVE = 0,		 	// Only 1 task will ever be active
	READY,				// Task may be selected to be active
	SEND_BLKD,			// Task is blocked on a send queue
	RCV_BLKD,			// Task is blocked on a receive queue
	RPLY_BLKD,			// Task is blocked waiting for a reply - It is not on a queue
	AWAITING_EVT,		// Task is blocked and awaiting an event
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
	volatile int spsr;		// Saved Processor State Register
	union {
		int volatile * volatile sp;	// Stack Pointer
		ReqArgs * volatile a;	// request arguments in a neatly avaiable union
	};
	int returnValue;		// the value to return after syscall
	int *sb;				// Stack base
	TID id;			 		// A unique identifying id
	TID parentId;			// The unique id of the parent

	int priority;			// A priority value (ranges from 0->2)
						
	enum TASK_STATE state;	// State of the task - see enum above

	TD *nextTD; 			// Link to the next TD in the TDM
	TD *prevTD; 			// Link to the prev TD in the TDM

	Queue sendQ;			// A circularly linked list of TDs that 
							// have called send() this TD a message
};

/**
 * The task descriptor manager.
 */
typedef struct {
	TD tdArray[NUM_TDS]; 		// Stores all the TDs

	TID lastId;					// Last id to used

	BitField empty[NUM_BITFIELD];// bitfield mask telling us which td's are used

    Queue ready[NUM_PRIORITY]; 	// The ready queue
	int highestPriority;		// The highest non-empty bucket in the ready Q

	Queue intBlocked[NUM_INTERRUPTS];	// A queue of blocked tasks awaiting interrupts
	Driver intDriver[NUM_INTERRUPTS];	// An array of drivers to handler each interrupt
} TDM;

/**
 * Initialize the priority queues and TDs in the td array.
 * mgr - The TDM to init.
 */
void mgr_init (TDM *mgr);

/**
 * Install a driver for the given eventid
 * mgr - The task descriptor manager
 * eventId - id of the event to install the driver for
 * driver - the driver to install 
 * RETURN: error code if failed
 */
int mgr_installDriver( TDM *mgr, int eventId, Driver driver );

/**
 * Create a new TD.
 * priority - The priority for the new TD.
 * start - The user task function.
 * parentId - The id of the parent who created this task.
 * mgr - The task descriptor manager.
 * RETURN: A pointer to the new TD.
 */
TD * td_create (int priority, Task start, TID parentId, TDM *mgr);

/**
 * Recycle the space associated witht a TD
 * td - the task descriptor that has exited
 * mgr - The prority queue manager
 */
void td_destroy (TD *td, TDM *mgr);

/**
 * Initialize a single td.
 * priority - The priority for the new TD.
 * start - The user task function.
 * parentId - The id of the parent who created this task.
 * mgr - The task descriptor manager.
 * RETURN: A pointer to the new TD.
 */
TD *td_init (int priority, Task start, TID parentId, TDM *mgr);

/**
 * Inserts the TD into an appropriate priority queue.
 * mgr - The task descriptor manager.
 * td - The task descriptor to insert.
 */
void mgr_insert (TDM *mgr, TD *td);

/**
 * Pops the highest priority TD off of the ready queue.
 * mgr - The task descriptor manager.
 * RETURN: The highest priority READY TD.
 */
TD *mgr_popReady (TDM *mgr);

/**
 * Fetches a TD from the task id.
 * mgr - The task descriptor manager.
 * tid - The id of the task to find.
 * RETURN: A pointer to the TD corresponding to the given tid.
 */
TD *mgr_fetchById (TDM *mgr, TID tid);

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

/**
 * Given a pointer to the head ofa a queue, and an entry in the queue
 * remove the entry from the queue and update the queue
 * head - A pointer to the head of the queue.
 * toRm - The TD to remove from the queue.
 */
void queue_remove ( Queue *head, TD *toRm );
#endif
