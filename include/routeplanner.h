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
	CONVERT_SENSOR,
	CONVERT_IDX,
	RESERVE,
	PLANROUTE,
	MIN_SENSOR_DIST // This must be last for error checking purposes
} RPType;

typedef struct {
	RPType	type;		// Route Planner request type
	
	// Shell
	char 	name[6];	// Node name to be converted
	
	int 	nodeIdx1;	// Predict sensors for this node OR 
						// Find Path from this node
	int 	nodeIdx2;	// to this node.

	// Train
	int 	sensor1;	// Find min distance from this sensor
	int 	sensor2;	// to this sensor.

	int		trainId;	// Unique id for train
	int     lastSensor;	// Last Hit Sensor Idx
	int   	avgSpeed;	// Average speed of a train
	int		destIdx;	// Destination Location Index
	
} RPRequest;

typedef struct {
	int len;
	int path[MAX_NUM_NODES];
} Path;

typedef struct {
	int len;
	int idxs[4];
} SensorsPred;

typedef struct {
	int dist;
	int id;
	SwitchDir dir;
} SwitchSetting;

typedef struct {
	union {
		int err;		// Check this first! If < 0, ERROR OCCURRED!
		int idx;		// Index of node (CONVERT return)
	};
} RPShellReply;

typedef struct {
	int err;			// Check this first! If < 0, ERROR OCCURRED!

	int stopDist;		// Distance to next stop
	SwitchSetting switches[3];
	SensorsPred   nextSensors;// Prediction for the next sensors

} RPReply;

void rp_run();

#endif

