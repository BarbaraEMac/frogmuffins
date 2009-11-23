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
#include "ui.h"

#define NUM_REQUESTS	64
#define	NUM_STRAY		256
#define	UNUSED_REQ		-1

#define POLL_SIZE 	10
#define POLL_WAIT 	(100 / MS_IN_TICK)
#define POLL_GRACE  (5000 / MS_IN_TICK) // wait 5 seconds before complaining
#define POLL_STR	(char [2]){133, 192}
#define	MATCH_ALL	99
#define SENSOR_WAIT	(50 / MS_IN_TICK)

/* FORWARD DECLARATIONS */
// ----------------------------------------------------------------------------
// A Track Detective
typedef struct {
	int			sensorHist[NUM_SENSORS];
	char		strayBuf[NUM_STRAY];
	RB			stray;
	DeRequest	requests[MAX_NUM_TRAINS];
	int			lstPoll;
	TID 	 	iosTid;
	TID  		csTid;
	TID			tsTid;
} Det;

int  det_init	( Det *det );
int  det_wake	( Det *det, int sensor, int ticks );	
int	 det_expire ( Det *det, int ticks );
void det_reply	( DeRequest *req, int sensor, int ticks );
void poll		();
void watchDog	();
// ----------------------------------------------------------------------------

/* ACTUAL CODE */
void det_run () {
	debug ("det_run: The Track Server is about to start. \r\n");	
	Det 		det;
	int			senderTid;
	DeRequest 	req;
	DeReply		reply;
	int			i, k, len, sensor;
	char		ch;
	UIRequest	uiReq;
	int 		uiTid;

	// Initialize the Track Server
	if_error( det_init (&det), "Initializing Track Server failed.");

	// Init UI Stuff
	uiTid = WhoIs( UI_NAME );
	uiReq.type = DETECTIVE;

	FOREVER {
		// Receive a server request
		len = Receive ( &senderTid, &req, sizeof(req) );
	
		assert( len == sizeof(DeRequest) );
		sensor = -1;
		
		switch (req.type) {
			case POLL:
				Reply ( senderTid, 0, 0 );
				for( i = 0; i < NUM_SENSOR_BANKS*2; i++ ) {
					for( k = 0; k < 8; k++ ) {
						if( req.rawSensors[i] & (0x80 >> k) ) {
							sensor = i*8 + k;

		printf("sensor %c%d\r\n", sensor_bank(sensor), sensor_num(sensor));

							// Send to the UI Server
							debug ("Detective: sending to the ui server\r\n");
							uiReq.time = req.ticks;
							uiReq.idx  = sensor;
							
							// Tell the UI about the triggered sensor
							Send( uiTid, (char*)&uiReq, sizeof(UIRequest), 
									 	 (char*)&uiReq.time, sizeof(int) );

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
				Reply ( senderTid, 0, 0 );
				
				//debug("det: Watchdog stamp %d ticks\r\n", req.ticks);
				if( det.lstPoll < req.ticks ) {
					error( TIMEOUT, "ts: Polling timed out. Retrying." );
					// Don't tell me for another 5 seconds
					det.lstPoll = req.ticks + POLL_GRACE;
					PutStr( POLL_STR, sizeof(POLL_STR), det.iosTid );
				}
				det_expire( &det, req.ticks );
				break;

			case WATCH_FOR:
				debug("det: WatchFor request for sensor(s):");
				for( i = 0; i < req.numEvents; i ++ ) {
					debug("%d", req.events[i].sensor);
				}
				debug(" expires at %d\r\n", req.expire);
				req.tid = senderTid;
				// Hash
				i = senderTid % MAX_NUM_TRAINS; // simple hash function
				assert( det.requests[i].type == UNUSED_REQ );
				det.requests[i] = req;
				break;

			case GET_STRAY:
				debug( "det: GetStray request from %d \r\n", senderTid );
				// Wait for next stray sensor
				req.tid = senderTid;
				req.events[0].sensor = MATCH_ALL;
				// Hash
				i = senderTid % MAX_NUM_TRAINS; // simple hash function
				if( det.requests[i].type != UNUSED_REQ ) {
					error( 0, "det: overwriting precious request." );
				}
				det.requests[i] = req;
				break;

			case RETRACT:
				debug( "det: Retract task (%d) by task (%d) \r\n", req.tid, senderTid );
				// Remove the request associated with this task
				// Hash
				i = req.tid % MAX_NUM_TRAINS; // simple hash function
				if( det.requests[i].type != UNUSED_REQ ) {
					// Reply properly
					det_reply( &det.requests[i], DET_RETRACTED, det.lstPoll );
					det.requests[i].type = UNUSED_REQ;
				}
				reply.ret = NO_ERROR;
				Reply ( senderTid, &reply, sizeof(reply) );
				break;

		
			default:
				reply.ret = DET_INVALID_REQ_TYPE;
				error (reply.ret, "Detective request type is not valid.");
				Reply ( senderTid, &reply, sizeof(reply) );
				break;
		}


	}
}

int det_init( Det *det ) {
	debug ("det_init: track detective=%x \r\n", det);
	int err = NO_ERROR;

	// Initialize the internal variables
	det->csTid   = WhoIs( CLOCK_NAME );
	det->iosTid  = WhoIs( SERIALIO1_NAME );
	det->tsTid   = WhoIs( TRACK_SERVER_NAME );
	det->lstPoll = POLL_GRACE; // wait 5 seconds before complaining

	rb_init(&(det->stray), det->strayBuf ) ;
//	rb_init(&(det->request), det->reqBuf, sizeof(DeRequest), MAX_NUM_TRAINS ) ;
	memoryset ( det->sensorHist, 0, sizeof(det->sensorHist) );

	DeRequest * req;
	foreach( req, det->requests ) {
		req->type = UNUSED_REQ;
	}

	// Create the helper tasks
	err =  Create( 3, &poll );
	if( err < NO_ERROR ) return err;
	err = Create( 3, &watchDog );
	if( err < NO_ERROR ) return err;
	
	// Register with the Name Server
	return RegisterAs( DETECTIVE_NAME );
}

void det_reply( DeRequest *req, int sensor, int ticks ) {
	debug("waking up (%d) with sensor %d at time %dms\r\n", req->tid, sensor, ticks * MS_IN_TICK);
	DeReply	rpl = { {sensor}, {ticks} };
	
	// Wake up the train
	 Reply( req->tid, (char*) &rpl, sizeof( TSReply ) );

	// Remove the request
	req->type = UNUSED_REQ;
}

int det_wake ( Det *det, int sensor, int ticks ) {
	debug ("det_wake: sensor: %d, time: %dms\r\n", sensor, ticks * MS_IN_TICK );
	DeRequest  *req;
	int			woken = 0, i;
	
	foreach( req, det->requests ) {
		for( i = 0; (req->type == WATCH_FOR) && (i < req->numEvents); i ++ ) {
			if(req->events[i].sensor == sensor ) {
				det_reply( req, sensor, ticks );
				woken++;
			}
		}
	}
	foreach( req, det->requests ) {
		if( req->type == GET_STRAY ) {
				det_reply( req, sensor, ticks );
		}
	}
	return woken;
}

int det_expire ( Det *det, int ticks ) {
	debug ("det_expire: time: %dms\r\n", ticks * MS_IN_TICK );
	DeRequest  *req;
	int			woken = 0;
	
	foreach( req, det->requests ) {
		if( (req->type != UNUSED_REQ) && (req->expire < ticks) ) {
			det_reply( req, TIMEOUT, ticks );
			woken++;
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
	DeRequest	req;
	int 		i, res;

	req.type = POLL;
	memoryset( raw, 0, POLL_SIZE );

	FOREVER {

		// Only updated ever so often
		//Delay( SENSOR_WAIT, csTid ); 

		// Poll the train box
		Purge( ioTid );
		PutStr( POLL_STR, sizeof(POLL_STR), ioTid );
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
		Send( deTid, &req, sizeof(req), 0, 0 );
	}
}

void watchDog () {
	debug( "det: poll watchdog task started\r\n" );

	TID deTid = MyParentTid();
	TID csTid = WhoIs( CLOCK_NAME );
	// We need the braces around the 0s because they're in unions
	DeRequest 	req;
	req.type = WATCH_DOG;

	FOREVER {
		// Watch dog timeout ever so often
		req.ticks = Time( csTid );
		Delay( POLL_WAIT, csTid );

		// Let the track server know
		Send( deTid, &req, sizeof(req), 0, 0 );
	}
}
