/*
 * CS 452: Track Detective User Task
 */
#define DEBUG 		1

#include <string.h>
#include <ts7200.h>

#include "debug.h"
#include "error.h"
#include "math.h"
#include "model.h"
#include "requests.h"
#include "servers.h"
#include "shell.h"
#include "string.h"
#include "task.h"
#include "trackserver.h"

#define NUM_REQUESTS	64

/* FORWARD DECLARATIONS */

/**
 * A Track Detective
 */
typedef struct {
	int			sensorHist[NUM_SENSORS];
	DetRequest	requests[MAX_NUM_TRAINS];
	TID 	 	iosTid;
	TID  		csTid;
	TID			tsTid;
} Det;


int det_init			( Det *det );
/*
int det_distance( Det *det, int sensorId ) {
	debug("det_distance: sensor:%d \r\n", sensorId );
	int idx = det->model.sensor_nodes[sensorId];

	Node *n = &det->model.nodes[idx];
	Edge *e;
	int dist = 0;
	SwitchDir dir = SWITCH_CURVED;
	if( sensorId % 2 ) {
		e = &n->se.behind;
	} else {
		e = &n->se.ahead;
	}

	while( 1 ) {
		dist += e->distance;
		printf("%s->", n->name);
		n = &det->model.nodes[e->dest];
		//printf("neighbour %s\r\n", n->name);
		if( n->type != NODE_SWITCH ) break;
		dir = det->switches[n->id];
		if( n->sw.behind.dest == idx ) {
			idx = e->dest;
			e = &n->sw.ahead[dir];
		} else {
			idx = e->dest;
			e = &n->sw.behind;
		}
	} 

	printf("%s:", n->name);
	//printf("distance from %d to %s is %d\r\n", sensorId, n->name, dist);
	return dist;
	
}*/

/* ACTUAL CODE */

void det_run () {
	debug ("det_run: The Track Server is about to start. \r\n");	
	Det 		det;
	int			senderTid;
	DetRequest 	req;
	TSReply		reply;
	int			i, k, len;


	// Initialize the Track Server
	if_error( det_init (&det), "Initializing Track Server failed.");

	FOREVER {
		// Receive a server request
		len = Receive ( &senderTid, (char *) &req, sizeof(req) );
	
		assert( len == sizeof(DetRequest) );
		
		switch (req.type) {
			case POLL:
				//debug("det: Poll results  %s\r\n", req.channel, req.rawSensors);
				for( i = 0; i < NUM_SENSOR_BANKS*2; i++ ) {
					for( k = 0; k < 8; k++ ) {
						if( req.rawSensors[i] & (1 << k) ) {
							debug("sensor i:%d k:%d\r\n", i, k);
							det.sensorHist[(i*8)+k] = req.ticks;
						}
					}
				}/*
				if( req.sensorId != det.lsdet.nsorId ) {
					dist = det_distance( &ts, det.lstSensorId );
					time = (req.ticks - det.lstSensorUpdate) * MS_IN_TICK;
					speed = (dist * 1000)/ time;
					printf("speed = %d/%d = \t%dmm/s\r\n", dist, time, speed);
					det.lstSensorUpdate = req.ticks;
				}*/
				break;

			case WATCH_FOR:
				debug("det: WatchFor request for sensor %d & %d\r\n",
						req.events[0].sensor, req.events[1].sensor);
				break;

		
			default:
				reply.ret = DET_INVALID_REQ_TYPE;
				error (reply.ret, "Track Server request type is not valid.");
				break;
		}

		Reply ( senderTid, (char *) &reply, sizeof(reply) );

	}
}

int det_init( Det *det ) {
	debug ("det_init: track detective=%x \r\n", ts);
	int err = NO_ERROR;

	det->csTid = WhoIs( CLOCK_NAME );
	det->iosTid = WhoIs( SERIALIO1_NAME );
	det->tsTid = WhoIs( TRACK_SERVER_NAME );
	
	memoryset ( (char *) det->sensorHist, 0, sizeof(det->sensorHist) );
	
	return err;
}
