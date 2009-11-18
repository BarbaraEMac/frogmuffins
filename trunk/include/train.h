/**
 * CS 451: Train User Task
 * becmacdo
 * dgoc
 */

#ifndef __TRAIN_H__
#define __TRAIN_H__


#include "requests.h"

typedef enum _train_mode {
	INIT_LOC = 1, 	// First go to a particular place
	INIT_DIR,		// then turn around to go around the loop in proper dir
	CAL_SPEED,		// Run around a bunch of times until speed is nice
	CAL_STOP,		// Calubrate stopping distance
	IDLE,			// Ready for input
	NORMAL			// Going from place A to place B
} TrainMode;

typedef struct {
	int			id;
	int			gear;
} TrainInit;

typedef struct {
	int  		dest;			// Desired End Location
	TrainMode 	mode;	// whether we're running calibration
} TrainCmd;


void train_run();

#endif

