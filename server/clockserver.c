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
	
	int 	   senderTid;		// The task id of the message sender
	CSRequest  req;				// A clock server request message
	int 	   ret, len;
	
	int       *currentTime = clock_init ( TIMER3_BASE, 1, 0, 0 );
	int		   curTimeInTicks;
	
	Sleeper   *sleepersQ;		// A queue of all sleeping user tasks
	Sleeper    allocdSleepers[NUM_SLEEPERS]; // For allocating memory
	int		   nextSleeper = 0;	// ptr into allocdSleepers array
	Sleeper   *tmpSleeper;		// tmp ptr

	// Initialize the Clock Server
	cs_init (&sleepersQ);
	assert( sleepersQ == 0 );

	FOREVER {
		// Receive a server request
		debug ("cs: is about to Receive. \r\n");
		len = Receive ( &senderTid, (char *) &req, sizeof(CSRequest) );
		debug ("cs: Received: fromTid=%d name='%s' type=%d  len=%d\r\n", 
				senderTid, req.name, req.type, len);
	
		assert( len == sizeof(CSRequest) );
		assert( senderTid > 0 );
		assert( req != 0 );
		assert( req.ticks >= 0 );
		
		// Calculate the current time in ticks in case a task wants to know
		curTimeInTicks  = 0xFFFFFFFF - *currentTime; // since the timer counts down
		curTimeInTicks /= 100;	// convert to ticks (where 50ms = 1 tick)
		assert( curTimeInTicks > 0 );

		// Handle the request
		switch (req.type) {
			case DELAY:
				debug ("cs: task %d delays %d ticks.\r\n", senderTid, ticks);
				tmpSleeper = &allocdSleepers[nextSleeper++];
	
				assert( nextSleeper < NUM_SLEEPERS );
				assert( tmpSleeper != 0 );
					
				tmpSleeper->endTime = *currentTime + (req.ticks * 100);
				tmpSleeper->tid     = senderTid;
				
				list_insert ( &(sleepersQ), tmpSleeper );
				break;
			
			case DELAYUNTIL:
				debug ("cs: task %d delays until %d ticks. (currently: %d)\r\n",
					   senderTid, ticks, currentTimeInTicks);
				tmpSleeper = &allocdSleepers [nextSleeper++];

				assert( nextSleeper < NUM_SLEEPERS );
				assert( tmpSleeper != 0 );
					
				tmpSleeper->endTime = req.ticks;
				tmpSleeper->tid     = senderTid;

				list_insert ( &(sleepersQ), tmpSleeper );
				break;
			
			case TIME:
				debug ("cs: task %d wants to know the current time: %d\r\n", 
					   senderTid, currentTimeInTicks);
				Reply (senderTid, (char*) &curTimeInTicks, sizeof(int));
				break;

			case NOTIFY:
				tmpSleeper = sleepersQ; // Can be 0 if nothing has delayed
				while ( tmpSleeper != 0 && *currentTime >= tmpSleeper->endTime ) {
					assert( tmpSleeper != 0 );
					
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
				error (ret, "Clockserver request type is not valid.");
				break;
		}
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
	
	assert( head != 0 );
	assert( toAdd != 0 );

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
	assert( toAdd->next != 0 );
	assert( toAdd->prev != 0 );
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

	int 	  err;
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
		if ( (err = AwaitEvent( TIMER1, awaitBuffer, sizeof(char)*10 )) 
				< NO_ERROR ) {
			// Handle errors
		
		}
	
		// Check the return buffer?


		// Tell the Clock Server 1 tick has passed
		if ( (err = Send( clockServerId, (char*) &req, sizeof(CSRequest),
					      &reply, sizeof(char) )) < NO_ERROR ) {
			// Handle errors
			
		}

		// The device driver will clear the interrupt
	}

	Exit(); // This will never be called.
}

void notifier_init () {
	// Register with the Name Server
	RegisterAs ("ClkNotifier");
	
	// Init clock stuff
	clock_init ( TIMER1_BASE, 1, 1, 100 );
}	
