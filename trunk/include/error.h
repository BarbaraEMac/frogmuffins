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
	NOT_FOUND,				// Name Server could not locate the corresponding task
	NS_INVALID_REQ_TYPE,	// Invalid Name Server request type
	CS_INVALID_REQ_TYPE		// Invalid Clock Server request type

} error;

#endif
