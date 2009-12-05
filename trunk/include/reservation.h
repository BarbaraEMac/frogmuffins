/**
 * CS 451: Reservation System
 * becmacdo
 * dgoc
 */

#ifndef __RESERVATION_H__
#define __RESERVATION_H__

#define RESERVATION_NAME 		"Reserver"

enum ResReqType {
	TRAIN_MAKE = 1,
	TRAIN_IDLE,
	TRAIN_STUCK,
	COURIER
};

typedef struct {
	enum ResReqType type;	// Request type
	int trainId;			// The train's identifying number
	int sensor;				// The last triggered sensor
	int distPast;			// The estimated distance past this sensor
	union {
		int stopDist;		// The estimated stopping distance
		int dest;			// Destination node index
	};
	int totalDist;			// The desired total distance to travel
} ResRequest;

typedef struct {
	int stopDist;			// The safe stopping distance for the train
} ResReply;

// The main server loop
void res_run ();

#endif
