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
	RCV_BUFFER_FULL,
	PQ_FULL,
	NULL_ADDR,


} error;


#endif
