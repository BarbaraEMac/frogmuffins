/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#define DEBUG 1

#include <ts7200.h>

#include "debug.h"
#include "drivers.h"
#include "error.h"
#include "requests.h"
#include "servers.h"

#define NUM_SLEEPERS	64
#define HARDWARE_TICKS	(MS_IN_TICK * 2)

//-----------------------------------------------------------------------------
/**
 * Any task that is sleeping on the clockserver will use this structure.
 */
typedef struct sleeper Sleeper;
struct sleeper {
	TID tid;		// The id of the sleeping task
	int endTime;	// The wake up time

	Sleeper *next;	// Pointer to next sleeper in the queue
	Sleeper *prev;	// Pointer to the previous sleeper in the queue
};

/**
 * Initialize the clock server.
 */
int cs_init (Sleeper **sleepersQ);

// List operators
void list_insert ( Sleeper **head, Sleeper *toAdd );

void list_remove ( Sleeper **head, Sleeper *toAdd );

// Notifier
/**
 * All of the Clock Server's notifier's work is done in here.
 */
void notifier_run();

/**
 * Initialize the Clock Server's Notifier.
 */
void notifier_init();
//-----------------------------------------------------------------------------

void cs_run () {
	debug ("cs_run: The Clock Server is about to start. \r\n");	
	
	int			 senderTid;					// The task id of the message sender
	CSRequest	 req;						// A clock server request message
	int			 len, idx;
	
	volatile int *time = clock_init ( TIMER3, 1, 0, 0 );
	int			 ticks;
	Sleeper		*sleepersQ;					// A queue of all sleeping user tasks
	Sleeper		 memSleepers[NUM_SLEEPERS]; // For allocating memory
	Sleeper		*sleeper;					// tmp ptr

	// Initialize the Clock Server
	int notifierTid = cs_init (&sleepersQ);

	// TODO move this into init
	for( idx = 0; idx < NUM_SLEEPERS; idx ++ ) {
		memSleepers[idx].tid = UNUSED_TID;
	}

	FOREVER {
		// Receive a server request
		debug ("cs: is about to Receive. \r\n");
		len = Receive ( &senderTid, (char *) &req, sizeof(CSRequest) );
		debug ("cs: Received: Tid=%d  type=%d len=%d\r\n", 
				senderTid, req.type, len);
	
		assert( len == sizeof(CSRequest) );
		assert( senderTid >= 0 );
		assert( req.ticks >= 0 );
		
		// Calculate the current time in ticks in case a task wants to know
		ticks  = 0 - *time; 		// since the timer counts down
		ticks /= HARDWARE_TICKS;	// convert to ticks (where 50ms = 1 tick)
		debug ("cs: time is %d\r\n", ticks);
		assert( ticks >= 0 );

		// Handle the request
		switch (req.type) {
			case DELAY:
				debug ("cs: task %d delays %d ticks. (currently: %d)\r\n", senderTid, req.ticks, ticks);
				idx = senderTid % NUM_SLEEPERS;
				sleeper = &memSleepers[idx];
				assert( sleeper->tid == UNUSED_TID );
					
				sleeper->endTime = ticks + req.ticks;
				sleeper->tid	 = senderTid;
				
				list_insert ( &(sleepersQ), sleeper );
				break;
			
			case DELAYUNTIL:
				debug ("cs: task %d delays until %d ticks. (currently: %d)\r\n",
					   senderTid, req.ticks, ticks);
				idx = senderTid % NUM_SLEEPERS;
				sleeper = &memSleepers[idx];
				assert( sleeper->tid == UNUSED_TID );

				sleeper->endTime = req.ticks;
				sleeper->tid     = senderTid;

				list_insert ( &(sleepersQ), sleeper );
				break;
			
			case TIME:
				debug ("cs: task %d wants to know the current time: %d\r\n", 
					   senderTid, ticks);
				Reply (senderTid, (char*) &ticks, sizeof(int));
				break;

			case NOTIFY:
				assert( notifierTid == senderTid );
				// Now wake up the Notifier!
				Reply (senderTid, (char*) &ticks, 0);

				sleeper = sleepersQ; // Can be 0 if nothing has delayed
				while ( sleeper != 0 && ticks >= sleeper->endTime ) {
					assert( sleeper != 0 );
					
					debug("cs: waking up tid: %d time: %d currentT: %d\r\n", sleeper->tid, sleeper->endTime, ticks);

					// Wake it up!
					Reply (sleeper->tid, (char*) &ticks, sizeof(int));
					
					// Remove from the list of sleeping tasks
					list_remove ( &(sleepersQ), sleeper );

					// Increment iterator pointer
					sleeper = sleeper->next;
				}

				break;
			
			default:
				error (CS_INVALID_REQ_TYPE, "Clockserver request type is not valid.");
				break;
		}
	}
	
	Exit(); // This will never be called.
}

int cs_init (Sleeper **sleepersQ) {
	int err;

	// Register with the Name Server
	RegisterAs (CLOCK_NAME);
	
	// Spawn a notifying helper task
	err = Create( NOTIFIER_PRTY, &notifier_run );
	if_error (err, "Cannot create the clock notifier.");

	// No tasks are currently waiting
	*sleepersQ = 0;

	return err;
}

void list_insert ( Sleeper **head, Sleeper *toAdd ) {
	
	assert( head != 0 );
	assert( toAdd != 0 );

	// The empty list
	if ( *head == 0 ) {
		debug ("	cs: add %d %d as head \r\n", toAdd->tid,toAdd->endTime);
		toAdd->next = toAdd;
		toAdd->prev = toAdd;

		*head = toAdd;
	}
	// New Element is the new head
	else if ( toAdd->endTime < (*head)->endTime ) {
		Sleeper *oldHead = *head;
		Sleeper *tail    = oldHead->prev;

		debug("		cs: add as new head %d %d (old: %d %d) \r\n", toAdd->tid, toAdd->endTime, oldHead->tid, oldHead->endTime);
		toAdd->next = oldHead;
		toAdd->prev = tail;

		oldHead->prev = toAdd;
		tail->next    = toAdd;
		
		// This is the new head!
		*head = toAdd;
	}
	// The new element needs to be inserted in the middle
	else {
		Sleeper *itr = (*head)->next;
		while ( toAdd->endTime > itr->endTime && itr != *head ) {
			assert ( itr != 0 );
			assert ( itr->next != 0 );
			assert ( itr->prev != 0 );
			
			itr = itr->next;
		}
	
		debug("		cs: add in middle to: %d %d prev: %d %d, next: %d %d\r\n", toAdd->tid, toAdd->endTime, itr->prev->tid, itr->prev->endTime, itr->prev->next->tid, itr->prev->next->endTime);
		// Insert in the middle
		toAdd->next = itr;
		toAdd->prev = itr->prev;

		itr->prev->next = toAdd;
		itr->prev       = toAdd;
	}


	debug ("\tcs: added tid: %d time: %d\r\n", toAdd->tid, toAdd->endTime);

	assert( *head != 0 );
	assert( toAdd->next != 0 );
	assert( toAdd->prev != 0 );
}

void list_remove ( Sleeper **head, Sleeper *toRm ) {
	debug ("\tcs: removing tid: %d time: %d\r\n", toRm->tid, toRm->endTime);
	
	// Remove the head - this is the last element
	if ( *head == toRm ) {
		Sleeper *newHead = toRm->next;
		Sleeper *tail = toRm->prev;

		// This is the last element
		if ( newHead == toRm ) {
			debug ("\tcs: removing last element (head).\r\n");
			*head = 0;	
		}
		else {
			debug ("\tcs: removing head. Adding next as new head.\r\n");
			newHead->prev = tail;
			tail->next    = newHead;

			*head = newHead;
		}
	}
	// Remove another element, not the head
	else {
		debug("\tcs: removing middle element\r\n");
		Sleeper *next = toRm->next;
		Sleeper *prev = toRm->prev;

		next->prev = prev;
		prev->next = next;
	}

	// Reset these links just incase something screwy happens
	toRm->next = 0;
	toRm->prev = 0;
	toRm->tid = UNUSED_TID;
}

void notifier_run() {
	debug ("notifier_run\r\n");

	int 	  err;
	int 	  clockServerId = MyParentTid();
	int 	  reply;
	char 	  awaitBuffer[1];
	CSRequest req;

	req.type  = NOTIFY;
	req.ticks = 0;

	// Initialize this notifier
	notifier_init ();
	
	FOREVER {
		// Wait for 1 "tick" (50ms) to pass
		err = AwaitEvent( TC1UI, awaitBuffer, sizeof(awaitBuffer) );
		if_error( err, "cs: awaitEvent failed." );

		// Tell the Clock Server 1 tick has passed
		err = Send( clockServerId, (char*) &req, sizeof(CSRequest),
					      (char *) &reply, sizeof(int) );
		if_error( err, "cs: send from notifier to server failed.");
		// The device driver will clear the interrupt
	}

	Exit(); // This will never be called.
}

void notifier_init () {
	debug ("notifier_init\r\n");
	
	// Register with the Name Server
	RegisterAs ("ClkNotifier");
	
	// Install the clock driver
	int err = InstallDriver( TC1UI, &timer1Driver );
	if_error( err, "cs: timer driver not installed.");

	// Init clock stuff
	clock_init ( TIMER1, 1, 1, HARDWARE_TICKS );
}	
