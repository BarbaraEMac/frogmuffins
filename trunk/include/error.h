/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __ERROR_H__
#define __ERROR_H__

enum ERROR {
	NO_ERROR = 0,
	NEG_TID = -1,
	NUM_TID = -2,
	DEFUNCT_TID = -3,
	OLD_TD = -4, 		// Not from the current generation
	OUT_OF_BOUNDS = -5,
	RCV_BUFFER_FULL = -6,
	RPLY_BUFFER_FULL = -7,
	PQ_FULL = -8,
	NULL_ADDR = -9,
	SNDR_NOT_RPLY_BLKD = -10,
	BUFFER_FULL = -11,
	INVALID_PRIORITY = -12,
	NO_TDS_LEFT = -13,
	INVALID_START_FCN = -14,
	

} error;

#define printErrorType (e) { bwprintf (COM2, "\r\nError: %s.", #e); }

#endif
