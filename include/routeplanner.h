/**
 * CS 451: Route Planner User Task
 * becmacdo
 * dgoc
 */

#ifndef __ROUTE_PLANNER_H__
#define __ROUTE_PLANNER_H__

#include "model.h"

typedef struct {
	int   speed;
	int   time;
	Node *currLoc;
	Node *dest;
} RPRequest;

void routeplanner_run();

#endif
