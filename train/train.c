/**
 * CS 452: User Task
 * becmacdo
 * dgoc
 */
#define DEBUG 1

#include <string.h>
#include <ts7200.h>

#include "debug.h"
#include "error.h"
#include "model.h"
#include "requests.h"
#include "servers.h"
#include "train.h"

// Private Stuff
// ----------------------------------------------------------------------------

typedef struct {
	int id;				// Identifying number
	int speed;			// The current speed
	Node currLoc;		// Current Location
	Node dest;			// Desired End Location
	
} Train;


// ----------------------------------------------------------------------------


void train_run () {
	
	// get the train number from shell
	// determine starting location from shell. Set as current location 
	// determine destination from shell
	
	// Until you've reached your destination:
	// talk to route planner
	// set speed to start on route
	// make prediction
	// verify prediction
	// calibrate speed profile

	Exit(); // When you've reached your destination
}

needs to:
talk to route planner to plan route from currLoc -> dest
talk to route planner to reserve some track
talk to detective to predict which track node it will hit next
talk to trackserver to set its speed & flip switches
calibrate its speed
talk to detective / track controller to ???
