/**
 * CS 451: Train User Task
 * becmacdo
 * dgoc
 */

#ifndef __TRAIN_H__
#define __TRAIN_H__


#include "requests.h"

typedef enum _train_mode {
//	INIT_LOC = 1, 	// First go to a particular place
//	INIT_DIR,		// then turn around to go around the loop in proper dir
//	CAL_SPEED,		// Run around a bunch of times until speed is nice
//	CAL_STOP,		// Calibrate stopping distance
//	CAL_WAIT,		// Waiting for the train to stop
	CAL_SPEED = 1,	// Calibrate the running speed
	CAL_STOP,		// Calibrate the stopping distance
	IDLE,			// Ready for input
	NORMAL			// Going from place A to place B
} TrainMode;

typedef enum _train_request {
	TRAIN_INIT = 0,
	DEST_UPDATE,
	TIME_UPDATE,
	POS_UPDATE,
	STOP_UPDATE,
	STRAY_TIMEOUT,
	WATCH_TIMEOUT,
	CALIBRATE
} TrainCode;

typedef struct {
	TrainCode	type;
	union	{ 
		int  		dest;			// Desired End Location
		int			sensor;			// From detective
		int			id;				// Destination
		int			mm;
	};
	union {
		TrainMode 	mode;			// whether we're running calibration
		int			ticks;			// From Detective
		int			gear;			// default gear
	};
} TrainReq;


void train_run();

#endif

