/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
//#define DEBUG

#include <bwio.h>
#include <clock.h>
#include <ts7200.h>

#include "clockserver.h"
#include "debug.h"
#include "error.h"
#include "requests.h"

#define NUM_SLEEPERS	1024

void cs_run () {
	debug ("cs_run: The Clock Server is about to start. \r\n");	
	Sleeper   *sleepersQ;		// A queue of all sleeping user tasks
	int 	   senderTid;		// The task id of the message sender
	CSRequest  req;				// A clock server request message
	int 	   ret, len;
	int       *currentTime = clock_init ( TIMER3_BASE, 1, 0, 0 );
	int		   curTimeInTicks;
	Sleeper    allocdSleepers[NUM_SLEEPERS]; // For allocating memory
	int		   nextSleeper = 0;	// ptr into allocdSleepers array
	Sleeper   *tmpSleeper;		// tmp ptr

	// Initialize the Clock Server
	cs_init (&sleepersQ);

	FOREVER {
		// Receive a server request
		debug ("cs: is about to Receive. \r\n");
		len = Receive ( &senderTid, (char *) &req, sizeof(CSRequest) );
		debug ("cs: Received: fromTid=%d name='%s' type=%d  len=%d\r\n", 
				senderTid, req.name, req.type, len);
	
		assert( len == sizeof(CSRequest) );

		switch (req.type) {
			case DELAY:
				tmpSleeper = &allocdSleepers[nextSleeper++];

				tmpSleeper->endTime = *currentTime + (req.ticks * 100);
				tmpSleeper->tid     = senderTid;
				
				list_insert ( &(sleepersQ), tmpSleeper );
				break;
			
			case DELAYUNTIL:
				tmpSleeper = &allocdSleepers [nextSleeper++];

				tmpSleeper->endTime = req.ticks;
				tmpSleeper->tid     = senderTid;

				list_insert ( &(sleepersQ), tmpSleeper );
				break;
			
			case TIME:
				// Return the number of ticks since starting
				curTimeInTicks  = 0xFFFFFFFF - *currentTime; // since the timer counts down
				curTimeInTicks /= 100;	// convert to ticks (where 50ms = 1 tick)
				Reply (senderTid, (char*) &curTimeInTicks, sizeof(int));
				
				break;

			case NOTIFY:
				curTimeInTicks  = 0xFFFFFFFF - *currentTime; // since the timer counts down
				curTimeInTicks /= 100;	// convert to ticks (where 50ms = 1 tick)
				tmpSleeper = sleepersQ;

				while ( *currentTime >= tmpSleeper->endTime ) {
					// Remove from the list of sleeping tasks
					list_remove ( &(sleepersQ), tmpSleeper );

					// Wake it up!
					Reply (senderTid, (char*) &curTimeInTicks, sizeof(int));

					// Increment iterator pointer
					tmpSleeper = tmpSleeper->next;
				}

				break;
			
			default:
				ret = CS_INVALID_REQ_TYPE;
				debug (1, "Clockserver request type is not valid.");
				break;
		}

		debug ("cs: about to Reply to %d. \r\n", senderTid);
		Reply ( senderTid, (char *) &ret, sizeof(int) );
		debug ("cs: returned from Replying to %d. \r\n", senderTid);
	}
	
	Exit(); // This will never be called.
}

void cs_init (Sleeper **sleepersQ) {
	int err;

	// Register with the Name Server
	RegisterAs (CLOCK_NAME);
	
	// Spawn a notifying helper task
	if ( (err = Create( 1, &notifier_run )) < NO_ERROR ) {
		error (err, "Cannot create the clock notifier.\r\n");
	}

	// No tasks are currently waiting
	*sleepersQ = 0;
}

void list_insert ( Sleeper **head, Sleeper *toAdd ) {
	
	// The empty list
	if ( *head == 0 ) {
		toAdd->next = toAdd;
		toAdd->prev = toAdd;

		*head = toAdd;
	}
	// New Element is the new head
	else if ( toAdd->endTime < (*head)->endTime ) {
		Sleeper *oldHead = *head;
		Sleeper *tail    = oldHead->prev;

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
	
		// Insert in the middle
		toAdd->next = itr;
		toAdd->prev = itr->prev;

		itr->prev->next = toAdd;
		itr->prev       = toAdd;
	}

	assert ( *head != 0 );
}

void list_remove ( Sleeper **head, Sleeper *toRm ) {
	
	// Remove the head - this is the last element
	if ( *head == toRm ) {
		*head = 0;	
	}
	// Remove another element, not the head
	else {
		Sleeper *next = toRm->next;
		Sleeper *prev = toRm->prev;

		next->prev = prev;
		prev->next = next;
	}

	// Reset these links just incase something screwy happens
	toRm->next = 0;
	toRm->prev = 0;
}

void notifier_run() {

	int 	  tick, error;
	int 	  clockServerId = MyParentTid();
	char 	  reply;
	char 	  awaitBuffer[10];
	CSRequest req;

	req.type  = NOTIFY;
	req.ticks = 0;

	// Initialize this notifier
	notifier_init ();
	
	FOREVER {
		// Wait for 1 "tick" (50ms) to pass
		tick = AwaitEvent ( TIMER, awaitBuffer, sizeof(char)*10 );
		
		// Check the return buffer?
		
		// Handle errors
		if ( tick < NO_ERROR ) {

		}

		// Tell the Clock Server 1 tick has passed
		error = Send( clockServerId, (char*) &req, sizeof(CSRequest),
					  &reply, sizeof(char) );

		// Handle errors
		if ( error < NO_ERROR ) {

		}


	}
	Exit(); // This will never be called.
}

void notifier_init () {
	// Register with the Name Server
	RegisterAs ("ClkNotifier");
	
	// Init clock stuff
	clock_init ( TIMER1_BASE, 1, 1, 100 );
}	
