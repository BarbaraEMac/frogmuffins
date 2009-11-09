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


// ----------------------------------------------------------------------------

void routeplanner_run() {
	
	// Initialize shortest paths using model
	// Initialize track reservation system (nothing is reserved)
	
	// FOREVER 
	// Receive from client - current location, destination, speed?, current time?
	// Determine shortest path (observing blockages)
	// Return the speed and direction? Next landmark? (does train even know the track topology?)


}
