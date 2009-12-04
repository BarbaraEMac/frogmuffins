 /*
 * Shell for Kernel 
 * becmacdo
 * dgoc
 * Copied from course webpage
 */
#ifndef __MODEL_H__
#define __MODEL_H__

#include <debug.h>
#include <string.h>

#define NUM_SENSOR_BANKS 	5
#define SIZE_BANK			16
#define NUM_SENSORS			(NUM_SENSOR_BANKS * SIZE_BANK)
#define MAX_NUM_TRAINS 		10
#define MAX_NUM_NODES		80
#define MAX_NUM_EDGES		160
#define NODE_MAX_EDGES 		3
#define TRACK_A				0
#define	TRACK_B				1
#define	NODE_NAME_LEN		5

typedef enum {
	SWITCH_STRAIGHT = 0,
	SWITCH_CURVED = 1
} SwitchDir;

typedef struct _node Node;

typedef struct {
  //int 	dest;			// TODO: Remove this?
  int 	distance;

  Node *node1;			// just the index?
  Node *node2;

  int	node1ResDist;	// init to 0 - reserved distance in mm along edge
  int	node2ResDist;
} Edge;

typedef struct {
  Edge 		*ahead[2]; // (curved, straight)
  Edge 		*behind;
  SwitchDir set;
} Switch;

typedef struct {
  Edge 		*ahead;
  Edge 		*behind;
  Edge 		*filler[1];
} Sensor;

typedef struct {
  Edge 		*ahead;
  Edge 		*filler[2];
} Stop;

struct _node {
  enum {
    NODE_SWITCH = 3,
    NODE_SENSOR = 2,
    NODE_STOP = 1,
  } type;
  char name[NODE_NAME_LEN];
  int id, idx;
  union {
    Switch 	sw;
    Sensor 	se;
    Stop 	st;
    Edge 	*edges[3];
  };
  int reserved;
  int reserver;
  int x, y; // location (for GUI)
};

typedef struct {
  int 	num_nodes;
  int	num_edges;
  Node 	nodes[MAX_NUM_NODES];
  Edge	edges[MAX_NUM_EDGES];
  int 	sensor_nodes[NUM_SENSORS];
} TrackModel;

// Returns 0 on success, < 0 otherwise.
int parse_model(int trackId, TrackModel* model);

void free_model(TrackModel* model);


Node * node_neighbour( Node *node, Edge *edge );

/**
 * Given a current location and the previous node passed,
 * will return the next node you will pass.
 * If the current node is a switch and you are heading along the 
 * behind edge, the next 2 possible nodes are returned.
 * Otherwise, next2 will be 0.
 */
/*void model_findNextNodes( TrackModel *model, Node *curr, Node *prev,
						  Node *next1, Node *next2 );*/
	
/**
 * Converts a node name into the index in O(n) time.
 * Returns NOT_FOUND if the name is not a valid node name.
 */
int model_nameToIdx (TrackModel *model, const char *name);

/**
 * Converts a given index into the node name.
 */
//char *model_idxToName (TrackModel *model, int idx);


char sensor_bank( int sensor );
char sensor_num( int sensor );
char switch_dir( SwitchDir dir );
SwitchDir switch_init ( char dir );

// Return the distance between two neighbouring nodes
int   node_dist		( Node *n1, Node *n2 );

Node *node_neighbour( Node *node, Edge *edge );

// Convert a sensor index into a node index
inline int sIdxToIdx ( int sIdx );

// Convert a node and name into a sensor index
inline int idxTosIdx (int idx, char *name);

#endif
