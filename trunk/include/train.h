/**
 * CS 451: Train User Task
 * becmacdo
 * dgoc
 */

#ifndef __TRAIN_H__
#define __TRAIN_H__


#include "requests.h"

typedef enum _train_mode {
	NORMAL = 1,
	CALIBRATION = 2
} TrainMode;

typedef struct {
	int  id;			// Identifying number
	int  currLoc;		// Current Location
	int  dest;			// Desired End Location
	TrainMode mode;	// whether we're running calibration
} TrainInit;

void train_run();

#endif

