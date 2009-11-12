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
	Node 	*nodeA;		// Current Sensor, Start Location
	Node 	*nodeB; 	// Previous Sensor, Destination

	int		idx1;
	int 	idx2;
} RPRequest;

typedef struct {
	int len;
	int path[MAX_NUM_NODES];
} Path;

typedef struct {
	
	int totalDist;			// stopping distance
	int checkinDist;		// Distance to next switch
	
	Path	path;
} RPReply;

void rp_run();

#endif

