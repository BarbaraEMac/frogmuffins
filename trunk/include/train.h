/**
 * CS 451: Train User Task
 * becmacdo
 * dgoc
 */

#ifndef __TRAIN_H__
#define __TRAIN_H__


#include "requests.h"

typedef struct {
	int  id;			// Identifying number
	int  currLoc;		// Current Location
	int  dest;			// Desired End Location
} TrainInit;

void train_run();

#endif

