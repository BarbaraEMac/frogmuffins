/*
 * CS 452: Track Detective User Task
 */
#define DEBUG 		1

#include <string.h>
#include <buffer.h>

#include "debug.h"
#include "error.h"
#include "math.h"
#include "requests.h"
#include "servers.h"
#include "shell.h"
#include "string.h"
#include "task.h"
#include "trackserver.h"

#define NUM_REQUESTS	64
#define	NUM_STRAY		256

#define POLL_SIZE 	10
#define POLL_WAIT 	(100 / MS_IN_TICK)
#define POLL_GRACE  (5000 / MS_IN_TICK) // wait 5 seconds before complaining
#define POLL_CHAR	133



/* FORWARD DECLARATIONS */

/**
 * A Track Detective
 */
typedef struct {
	int			sensorHist[NUM_SENSORS];
	char		strayBuf[NUM_STRAY];
	RB			stray;
	int			lstPoll;
	DetRequest	requests[MAX_NUM_TRAINS];
	TID 	 	iosTid;
	TID  		csTid;
	TID			tsTid;
} Det;


int  det_init	( Det *det );
int  det_wake	( Det *det, int sensorId );	
void poll		();
void watchDog	();

/* ACTUAL CODE */

void det_run () {
	debug ("det_run: The Track Server is about to start. \r\n");	
	Det 		det;
	int			senderTid;
	DetRequest 	req;
	TSReply		reply;
	int			i, k, len, sensorId;
	char		ch;


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
						if( req.rawSensors[i] & (0x80 >> k) ) {
							printf("sensor i:%d k:%d %c%d\r\n", i, k, 'A'+(i/2), 1+k+8*(i%2));
							sensorId = i*8 + k;
							// Update the history
							det.sensorHist[sensorId] = req.ticks;
							// Wake up any tasks waiting for this event
							if( !det_wake( &det, sensorId ) ) {
								printf( "rb size %d\r\n", det.stray.size );
								if( rb_full( &det.stray ) ) {
									ch = *(char*)(rb_pop( &det.stray ));
									printf( "poping sensor #%d\r\n", ch );
								}
								ch = (char) sensorId;
								rb_push( &det.stray, &ch );
								// put in stray sensor queue
							}
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
				det.lstPoll = req.ticks;
				break;

			case WATCH_DOG:
				debug("det: Watchdog stamp %d ticks\r\n", req.ticks);
				if( det.lstPoll < req.ticks ) {
					error( TIMEOUT, "ts: Polling timed out. Retrying." );
					// Don't tell me for another 5 seconds
					det.lstPoll = req.ticks + POLL_GRACE;
					Putc( POLL_CHAR, det.iosTid );
				}
				break;


			case WATCH_FOR:
				debug("det: WatchFor request for sensor %d & %d\r\n",
						req.events[0].sensor, req.events[1].sensor);
				req.tid = senderTid;
				// Enqueue
				// TODO
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
	debug ("det_init: track detective=%x \r\n", det);
	int err = NO_ERROR;

	det->csTid = WhoIs( CLOCK_NAME );
	det->iosTid = WhoIs( SERIALIO1_NAME );
	det->tsTid = WhoIs( TRACK_SERVER_NAME );
	det->lstPoll = POLL_GRACE; //wait 5 seconds before complaining

	rb_init(&(det->stray), det->strayBuf, NUM_STRAY, sizeof(char)) ;
	
	memoryset ( (char *) det->sensorHist, 0, sizeof(det->sensorHist) );
	
	err =  Create( 3, &poll );
	if( err < NO_ERROR ) return err;
	err = Create( 3, &watchDog );
	if( err < NO_ERROR ) return err;
	return RegisterAs( TRACK_DETECTIVE_NAME );
}

int det_wake ( Det *det, int sensorId ) {


	return 0;
}

void poll() {
	debug( "det: polling task started\r\n" );

	TID detTid = MyParentTid();
	TID csTid = WhoIs( CLOCK_NAME );
	TID ioTid = WhoIs( SERIALIO1_NAME );
	char ch, raw[POLL_SIZE];
	// We need the braces around the 0s because they're in unions
	DetRequest	req;
	TSReply		rpl;
	int 		i, res;

	req.type = POLL;
	memoryset( raw, 0, POLL_SIZE );

	FOREVER {

		// Only updated ever so often
		//Delay( SNSR_WAIT, csTid );

		// Poll the train box
		Putc( POLL_CHAR, ioTid );
		for( i = 0; i < POLL_SIZE; i++ ) {
			res = Getc( ioTid );
			if( res < NO_ERROR ) {
				error( res, "det: Polling train box failed" );
				break;
			}
			ch = (char) res;
			req.rawSensors[i] = ch & ~(raw[i]);
			raw[i] = ch;
		}
		req.ticks = Time( csTid );

		// Let the track server know
		Send( detTid, (char *) &req, sizeof(req), (char*) &rpl, sizeof(rpl));
	}
}

void watchDog () {

	debug( "det: poll watshdog task started\r\n" );

	TID detTid = MyParentTid();
	TID csTid = WhoIs( CLOCK_NAME );
	// We need the braces around the 0s because they're in unions
	DetRequest 	req;
	TSReply		rpl;
	req.type = WATCH_DOG;

	FOREVER {
		// Watch dog timeout ever so often
		req.ticks = Time( csTid );
		Delay( POLL_WAIT, csTid );

		// Let the track server know
		Send( detTid, (char *) &req, sizeof(req), (char*) &rpl, sizeof(rpl));
	}
}

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
