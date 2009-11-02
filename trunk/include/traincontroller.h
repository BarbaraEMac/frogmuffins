/**
 * CS 451: Train Controller User Task
 * becmacdo
 * dgoc
 */

#ifndef __TRAIN_CONTROLLER_H__
#define __TRAIN_CONTROLLER_H__

#define TRAIN_CONTROLLER_NAME "TrainCtrl"
#define NUM_TRNS    81
#define NUM_SWTS    256
#define SW_CNT      22
#define SW_WAIT     100
#define SNSR_WAIT   100
#define TRAIN_WAIT  500
#define TRAIN_TRIES 5

#include "requests.h"

enum TCRequestCode {
	RV = 1,
	ST,
	SW,
	TR,
	WH
};

typedef struct {
	enum TCRequestCode type;
	union {
		int train;
		int sw;
		int arg1;
	};
	union {
		int speed;
		char dir;
		int arg2;
	};
} TCRequest;



/**
 * The main function for the Train Controller task.
 */
void tc_run ();

#endif
