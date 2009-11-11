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
	RESERVE = 1,
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
} RPRequest;

typedef struct {
	int len;
	int path[MAX_NUM_NODES];
} Path;

typedef struct {
	
	int dist;			// stopping distance
	int swDist;			// Distance to next switch
	
	Path	path;
} RPReply;

void rp_run();

#endif

