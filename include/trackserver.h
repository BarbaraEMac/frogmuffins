/**
 * CS 451: Track Server User Task
 * becmacdo
 * dgoc
 */

#ifndef __TRACK_SERVER_H__
#define __TRACK_SERVER_H__

#define TRACK_SERVER_NAME	"TrackSvr"

#include "requests.h"

enum TSRequestCode {
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
	enum TSRequestCode type;
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
} TSRequest;


typedef struct { 
	struct {
		char sensor;
		char channel;
		char dir;
	};
	int ret;
	int ticks;		// hopefully this is never more than 255
} TSReply;

/**
 * The main function for the Track Server task.
 */
void ts_run ();

#endif
