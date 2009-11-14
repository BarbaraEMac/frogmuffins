/**
 * CS 451: Route Planner User Task
 * becmacdo
 * dgoc
 */

#ifndef __ROUTE_PLANNER_H__
#define __ROUTE_PLANNER_H__

#include "model.h"

#define ROUTEPLANNER_NAME	"RoutePlnr"

typedef enum RPType {
	ERROR = 0,
	DISPLAYROUTE = 1,
	DISPLAYFSTSW,
	DISPLAYFSTRV,
	DISPLAYPREDICT,
	CONVERT,
	RESERVE,
	PLANROUTE,
	MINDIST,
	NEIGHBOURDIST	// This must be last for error checking purposes
} RPType;

typedef struct {
	RPType	type;		// Route Planner request type
	
	int		trainId;	// Unique id for train
	int   	avgSpeed;	// Average speed of a train
						// between the past sensors
	int		idx1; 		// Current Sensor, Start Location
	int 	idx2; 		// Previous Sensor

	char 	name[3];	// Node name to be converted

	int		dest;		// Destination Location Idx
} RPRequest;

typedef struct {
	int len;
	int path[MAX_NUM_NODES];
} Path;

typedef struct {
	int len;
	int s[4];
} Sensors;

typedef struct {
	union {
		int err;		// Check this first! If < 0, ERROR OCCURRED!
		int idx;		// Index of node
		int totalDist;	// stopping distance
		int minDist;	// Minimum distance b/w nodes
	};
	
	int checkinDist;	// Distance to next checkin location

	Path path;			// Path of indices from s -> t
	Sensors nextSensors;// Prediction for the next sensors

	int reverse;		// 1 if need to reverse; Garbage otherwise;

	int switchId;		// Id of the next switch
	SwitchDir switchDir;// 'S' OR 'C' if the switch needs to be thrown
						// 0 		  Otherwise
} RPReply;

void rp_run();

#endif

