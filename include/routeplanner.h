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
	DISPLAYROUTE = 1,
	DISPLAYFSTSW,
	DISPLAYFSTRV,
	DISPLAYPREDICT,
	RESERVE,
	PLANROUTE,
	MINDIST,
	NEIGHBOURDIST
} RPType;

typedef struct {
	RPType	 type;		// Route Planner request type
	
	int		 trainId;	// Unique id for train
	int   	 avgSpeed;	// Average speed of a train
						// between the past sensors

	int		idx1; 		// Current Sensor, Start Location
	int 	idx2; 		// Previous Sensor, Destination
} RPRequest;

typedef struct {
	int len;
	int path[MAX_NUM_NODES];
} Path;

typedef struct {
	union {
		int totalDist;	// stopping distance
		int minDist;	// Minimum distance b/w nodes
	};
	int checkinDist;	// Distance to next switch
	
	Path	path;
} RPReply;

void rp_run();

#endif

