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
	CALIBRATION = 2,
	INIT = 3,
	IDLE = 4
} TrainMode;

typedef struct {
	int			id;
} TrainInit;

typedef struct {
	int  		dest;			// Desired End Location
	TrainMode 	mode;	// whether we're running calibration
} TrainCmd;


void train_run();

#endif

