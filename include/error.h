/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __ERROR_H__
#define __ERROR_H__

enum ERROR {
	NO_ERROR = 0,
	NEG_TID = -10000,
	DEFUNCT_TID,
	OLD_TID, 		// Not from the current generation
	INVALID_TID,
	OUT_OF_BOUNDS,
	RCV_BUFFER_FULL,
	RPLY_BUFFER_FULL,
	PQ_FULL,
	NULL_ADDR,
	SNDR_NOT_RPLY_BLKD,
	BUFFER_FULL,
	INVALID_PRIORITY,
	NO_TDS_LEFT,
	INVALID_START_FCN,
	NAME_NOT_REGISTERED,
	NOT_FOUND,
	

} error;

#endif
