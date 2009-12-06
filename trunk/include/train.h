/**
 * CS 451: Train User Task
 * becmacdo
 * dgoc
 */

#ifndef __TRAIN_H__
#define __TRAIN_H__

#include "requests.h"

#define	START_SENSOR			-1

typedef enum _train_mode {
	CAL_SPEED = 1,	// Calibrate the running speed
	CAL_STOP,		// Calibrate the stopping distance
	IDLE,			// Ready for input
	DRIVE,			// Going from place A to place B
	WAITING			// Waiting for a clear path
} TrainMode;

typedef enum _train_request {
	TRAIN_INIT = 0,
	DEST_UPDATE,
	TIME_UPDATE,
	POS_UPDATE,
	STOP_UPDATE,
	STRAY_TIMEOUT,
	WATCH_TIMEOUT
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

