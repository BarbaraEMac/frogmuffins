/**
 * CS 451: Track Server User Task
 * becmacdo
 * dgoc
 */

#ifndef __TRACK_SERVER_H__
#define __TRACK_SERVER_H__

#define TRACK_SERVER_NAME	"TrackSvr"
#define TRACK_DETECTIVE_NAME "Watson"

#include "requests.h"
#include "model.h"

enum TSRequestCode {
	RV = 1,
	ST,
	SW,
	TR,
	WH,
	POLL,
	WATCH_DOG,	// detective only
	START,
	STOP,
	WATCH_FOR,	// detective only
	GET_STRAY	// detective only
};

typedef struct {
	enum TSRequestCode type;
	union {
		int train;
		int sw;
		int sensor;
		int arg1;
	};
	union {
		int speed;
		SwitchDir dir;
		//char sensor;
		int ticks;
		int arg2;
	};
} TSRequest;


typedef struct { 
	union {
		int sensor;
		char dir;
	};
	union {
		int ret;
		int ticks;		// hopefully this is never more than 255
	};
} TSReply;

typedef struct {
	int sensor;
	int start; 	//in ticks
	int end;	//in ticks
} SensorWatch;

typedef TSReply DeReply;

typedef struct {
	union {
		enum TSRequestCode 	type;
		TID 		tid;
	};
	union {
		char 		rawSensors[NUM_SENSOR_BANKS*2];
		SensorWatch events[8];
	};
	union {
		int			ticks;
		int 		expire;
	};
	int			numEvents;
} DeRequest;



/**
 * The main function for the Track Server task.
 */
void ts_run ();

/*
 * The main fucntion for the Track Detective task.
 */
void det_run ();

#endif
