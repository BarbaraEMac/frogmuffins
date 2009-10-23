/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __ERROR_H__
#define __ERROR_H__

enum ERROR {
	
	NO_ERROR = 0,			// Everything ran smoothly!
	NEG_TID = -10000,		// The task id is negative
	DEFUNCT_TID,			// The corresponding td is defunct
	OLD_TID, 				// Not from the current generation
	INVALID_TID,			// The task id is not valid
	OUT_OF_BOUNDS,			// The address is not within the task's address space
	RCV_BUFFER_FULL,		// The Receive() buffer is full
	RPLY_BUFFER_FULL,		// The Reply() buffer is full
	NULL_ADDR,				// A buffer address is 0 (null)
	SNDR_NOT_RPLY_BLKD,		// The Sender is in a bad state
	INVALID_PRIORITY,		// The given priority is invalid
	NO_TDS_LEFT,			// No more useable TDs - Cannot "create" more
	INVALID_EVENTID,		// The event id is not valid
	NO_DRIVER,				// No driver is installed
	NOT_FOUND,				// Name Server could not locate the corresponding task
	NS_INVALID_REQ_TYPE,	// Invalid Name Server request type
	CS_INVALID_REQ_TYPE,	// Invalid Clock Server request type
	IOS_INVALID_REQ_TYPE,	// Invalid Serial IO request type
	TC_INVALID_REQ_TYPE,	// Invalid Train Controller request type
	INVALID_TRAIN,			// Invalid train number sent to train controller
	INVALID_DIR,			// Invalid direction for a train
	INVALID_SWITCH,			// Invalid switch number
	CONNECTION_TIMEOUT,		// Connection to the train controller failed
	CANNOT_INIT_SWITCHES	// Switch initialization failed
} error;

#endif
