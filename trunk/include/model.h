 /*
 * Shell for Kernel 
 * becmacdo
 * dgoc
 * Copied from course webpage
 */
#ifndef __MODEL_H__
#define __MODEL_H__

#define NUM_SENSOR_BANKS 	5
#define SIZE_BANK			16
#define NUM_SENSORS			(NUM_SENSOR_BANKS * SIZE_BANK)
#define MAX_NUM_TRAINS 		10
#define MAX_NUM_NODES		80
#define NODE_MAX_EDGES 		3
#define TRACK_A				0
#define	TRACK_B				1

typedef enum {
	SWITCH_STRAIGHT = 0,
	SWITCH_CURVED = 1
} SwitchDir;

typedef struct {
  int dest;
  int distance;
} Edge;

typedef struct {
  Edge 		ahead[2]; // (curved, straight)
  Edge 		behind;
  SwitchDir set;
} Switch;

typedef struct {
  Edge ahead;
  Edge behind;
  Edge filler[1];
  char trig_forward, trig_back;
} Sensor;

typedef struct {
  Edge ahead;
  Edge filler[2];
} Stop;

typedef struct {
  enum {
    NODE_SWITCH = 3,
    NODE_SENSOR = 2,
    NODE_STOP = 1,
  } type;
  char name[5];
  int id, idx;
  union {
    Switch sw;
    Sensor se;
    Stop st;
    Edge edges[3];
  };
  int reserved;
  int x, y; // location (for GUI)
} Node;

typedef struct {
  int 	num_nodes;
  Node 	nodes[MAX_NUM_NODES];
  int 	sensor_nodes[NUM_SENSORS];
} TrackModel;

// Returns 0 on success, < 0 otherwise.
int parse_model(int trackId, TrackModel* model);

void free_model(TrackModel* model);


/**
 * Given a current location and the previous node passed,
 * will return the next node you will pass.
 * If the current node is a switch and you are heading along the 
 * behind edge, the next 2 possible nodes are returned.
 * Otherwise, next2 will be 0.
 */
void model_findNextNodes( TrackModel *model, Node *curr, Node *prev,
						  Node *next1, Node *next2 ) {
	

#endif
