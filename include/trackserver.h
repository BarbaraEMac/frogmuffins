/**
 * CS 451: Train Controller User Task
 * becmacdo
 * dgoc
 */

#ifndef __TRAIN_CONTROLLER_H__
#define __TRAIN_CONTROLLER_H__

#define TRAIN_CONTROLLER_NAME "TrainCtrl"

#include "requests.h"

enum TCRequestCode {
	RV = 1,
	ST,
	SW,
	TR,
	WH,
	POLL,
	WATCH_DOG,
	START,
	STOP
};

typedef struct {
	enum TCRequestCode type;
	union {
		int train;
		int sw;
		struct {
			char channel;
			char sensor;
		};
		int arg1;
	};
	union {
		int speed;
		char dir;
		//char sensor;
		int timeStamp;
		int arg2;
	};
} TCRequest;


typedef struct { 
	struct {
		char sensor;
		char channel;
		char dir;
	};
	int ret;
	int ticks;		// hopefully this is never more than 255
} TCReply;

/**
 * The main function for the Train Controller task.
 */
void tc_run ();

#endif
