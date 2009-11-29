/**
 * CS 451: Reservation System
 * becmacdo
 * dgoc
 */

#ifndef __RESERVATION_H__
#define __RESERVATION_H__

#define RESERVATION_NAME 		"Reserver"

enum ResReqType {
	TRAIN_TASK = 1,
	ROUTE_PLANNER
};

typedef struct {
	enum ResReqType type;	// Request type
	int trainId;			// The train's identifying number
	int sensor;				// The last triggered sensor
	int distPast;			// The estimated distance past this sensor
	int stopDist;			// The estimated stopping distance
} ResRequest;

typedef struct {
	int stopDist;			// The safe stopping distance for the train
} ResReply;

// The main server loop
void res_run ();

#endif
