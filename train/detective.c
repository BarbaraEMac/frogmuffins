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
	DetRequest	requests[MAX_NUM_TRAINS];
	int			lstPoll;
	TID 	 	iosTid;
	TID  		csTid;
	TID			tsTid;

} Det;


int  det_init	( Det *det );
int  det_wake	( Det *det, int sensor, int ticks );	
void poll		();
void watchDog	();

/* ACTUAL CODE */

void det_run () {
	debug ("det_run: The Track Server is about to start. \r\n");	
	Det 		det;
	int			senderTid;
	DetRequest 	req;
	TSReply		reply;
	int			i, k, len, sensor;
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
							sensor = i*8 + k;
		printf("sensor %c%d\r\n", sensor_bank(sensor), sensor_num(sensor));
							// Update the history
							det.sensorHist[sensor] = req.ticks;
							// Wake up any tasks waiting for this event
							if( !det_wake( &det, sensor, req.ticks ) ) {
								// Put in stray sensor queue
								ch = (char) sensor;
								rb_force( &det.stray, &ch );
							}
						}
					}
				}
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
				i = senderTid % MAX_NUM_TRAINS; // simple hash function
				assert( det.requests[i].tid == UNUSED_TID );
				det.requests[i] = req;
				break;

			case GET_STRAY:
				debug("det: GetStray request from %d \r\n", senderTid);
				// Dequeue
				while( !rb_empty( &det.stray ) ) {
					sensor = *((int *) rb_pop( &det.stray ));	
					// TODO
				}
		
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

	rb_init(&(det->stray), det->strayBuf ) ;
//	rb_init(&(det->request), det->reqBuf, sizeof(DetRequest), MAX_NUM_TRAINS ) ;
	memoryset ( (char *) tr->sensorHist, 0, sizeof(tr->sensorHist) );

	DetRequest * req;
	foreach( req, det->requests ) {
		req->tid = UNUSED_TID;
	}

	err =  Create( 3, &poll );
	if( err < NO_ERROR ) return err;
	err = Create( 3, &watchDog );
	if( err < NO_ERROR ) return err;
	return RegisterAs( TRACK_DETECTIVE_NAME );
}

int det_wake ( Det *det, int sensor, int ticks ) {
	debug ("det_wake: sensor: %d, time: %d,s\r\n", sensor, ticks * MS_IN_TICK );
	DetRequest *req;
	TSReply		rpl;
	int			woken = 0;
	
	foreach( req, det->requests ) {
		if( req->tid != UNUSED_TID ) {
			if(req->events[0].sensor == sensor ||
			req->events[1].sensor == sensor ) {
				// Wake up the train
				rpl.sensor = sensor;
				rpl.ticks = ticks;
				Reply( req->tid, (char*) &rpl, sizeof( TSReply ) );
				// Remove the request
				req->tid = UNUSED_TID;
				woken++;
			}
		}
	}
	return woken;
}

void poll() {
	debug( "det: polling task started\r\n" );

	TID deTid = MyParentTid();
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
		Send( deTid, (char *) &req, sizeof(req), (char*) &rpl, sizeof(rpl));
	}
}

void watchDog () {

	debug( "det: poll watshdog task started\r\n" );

	TID deTid = MyParentTid();
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
		Send( deTid, (char *) &req, sizeof(req), (char*) &rpl, sizeof(rpl));
	}
}
