/*
 * CS 452: Track Server User Task
 */
#define DEBUG 		1

#define NUM_TRNS    81
#define NUM_SWTS    256
#define SW_CNT      22
#define SW_WAIT     2	// ticks
#define SNSR_WAIT   1	// ticks
#define TRAIN_WAIT	10	// ticks
#define	START_STR	(char [2]){96, 192}
#define STOP_STR	(char [1]){97}
#include <string.h>

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
#include "ui.h"

/* FORWARD DECLARATIONS */

/**
 * A Track Server
 */
typedef struct {
    char 		lstSensor;
	int  		lstSensorUpdate; // int ticks
	int  		lstSensorPoll;
    char 		speeds   [NUM_TRNS];
    SwitchDir 	switches [NUM_SWTS];
	TID 	 	iosTid;
	TID  		csTid;
	TID			uiTid;
} TS;

int	ts_start		( TS *ts );
int	ts_stop			( TS *ts );
int	ts_init			( TS *ts );
int	ts_trainSet		( TS *ts, int train, int speed );
int	ts_switchSet	( TS *ts, int sw, SwitchDir dir );
int	ts_switchSetAll	( TS *ts, SwitchDir *dirLow, SwitchDir *dirHigh );
int	checkTrain		( int train );

/* ACTUAL CODE */

void ts_run () {
	debug ("ts_run: The Track Server is about to start. \r\n");	
	TS 			ts;
	int			senderTid;
	TSRequest 	req;
	TSReply		reply;
	int			speed, len;
	
	// Initialize the Track Server
	if_error( ts_init (&ts), "Initializing Track Server failed.");

	FOREVER {
		// Receive a server request
		len = Receive ( &senderTid, (char *) &req, sizeof(req) );
//		printf ("Current Time: %d Request Sent At: %d\r\n", Time(WhoIs(CLOCK_NAME)), req.startTicks);
	
		assert( len == sizeof(TSRequest) );
		
		switch (req.type) {
			case RV:
				if ( (reply.ret = checkTrain ( req.train )) >= NO_ERROR ) {
					speed = ts.speeds[req.train];
					
					reply.ret =  ts_trainSet( &ts, req.train, 0 );
					// Wait for slow down depending on previous speed 
					// TODO is the server allowed to block like this?
					Delay( speed, ts.csTid );
					reply.ret |= ts_trainSet( &ts, req.train, 15 );
					reply.ret |= ts_trainSet( &ts, req.train, speed );
				}
				
				break;
			case ST:
				reply.dir = ts.switches[req.sw]; 
				break;

			case SW:
				reply.ret = ts_switchSet( &ts, req.sw, req.dir );
				break;
			
			case TR:
				reply.ret = ts_trainSet( &ts, req.train, req.speed );
				break;

			case WH:
				reply.sensor = ts.lstSensor;
				reply.ticks = (Time( ts.csTid ) - ts.lstSensorPoll);
				break;

			case POLL:
				if( req.sensor != ts.lstSensor ) {
					ts.lstSensorUpdate = req.ticks;
				}
				ts.lstSensor = req.sensor;
				ts.lstSensorPoll = req.ticks;
				break;

			case START:
				ts_start( &ts );
				break;

			case STOP:
				ts_stop( &ts );
				break;
			
			default:
				reply.ret = TS_INVALID_REQ_TYPE;
				error (reply.ret, "Track Server request type is not valid.");
				break;
		}

		Reply ( senderTid, (char *) &reply, sizeof(reply) );
	}
}

int ts_init( TS *ts ) {
	debug ("ts_init: track server=%x \r\n", ts);
	int i;
	int err = NO_ERROR;
	SwitchDir dirLow[NUM_SWTS - 4];
	SwitchDir dirHigh[4];

	ts->lstSensor  = -1;
	ts->lstSensorUpdate = 0;
	ts->csTid = WhoIs( CLOCK_NAME );
	ts->iosTid = WhoIs( SERIALIO1_NAME );
	ts->uiTid = WhoIs (UI_NAME);
	
	ts_start( ts );
	
	// Stop the trains
	int trains[] = {12, 22,  24, 49, 52};
	int *k;
	foreach( k, trains ) {
		ts_trainSet( ts, *k, 0 );
		ts_trainSet( ts, *k, 64 );
	}

	// Reset the switches
	memoryset ( ts->speeds, 0, NUM_TRNS );
	
	for ( i = 0; i < NUM_SWTS - 4; i ++ ) {
		if ( i == 11 ) {
			dirLow[i] = SWITCH_CURVED;
		} else {
			dirLow[i] = SWITCH_STRAIGHT;
		}
	}
	
	for ( i = 0; i < 4; i ++ ) {
		dirHigh[i] = SWITCH_CURVED;	
	}
	
	ts_switchSetAll( ts, dirLow, dirHigh );
	
	err = Create(OTH_SERVER_PRTY, &det_run );
	if( err < NO_ERROR ) return err;
	err = RegisterAs( TRACK_SERVER_NAME );
	return err;
}

int ts_start( TS *ts ) {
	return PutStr( START_STR, sizeof(START_STR), ts->iosTid );
}

int ts_stop( TS *ts ) {
	return PutStr( STOP_STR, sizeof(STOP_STR), ts->iosTid );
}

// check if the train index is within range
int checkTrain( int train ) {
    if (train >= 0 && train < NUM_TRNS) {
		return NO_ERROR;
	}

	debug ("ts: Invalid train value %d.\r\n", train);
	return INVALID_TRAIN; 
}

// send commands to the train, try a few times
int ts_trainSet( TS *ts, int train, int speed ) {
    debug ("ts: trainSet: tr=%d sp=%d\r\n", train, speed);

	// check the train number is valid
	int err = checkTrain( train );
	if( err >= NO_ERROR ) {
		char bytes[2] = { (char) speed, (char) train };

		// Send the command
		err = PutStr( bytes, sizeof(bytes), ts->iosTid);
		if_error( err, "Sending to track server failed." );

		// Store the new train speed
		ts->speeds[train] = (char) speed;
	}
    return err;
}

// set a switch to a desired position checking for bad input
int ts_switchSet( TS *ts, int sw, SwitchDir dir ) {
    debug ("ts: switchSet: sw=%d dir=%c\r\n", sw, switch_dir (dir) );
	int ret = NO_ERROR;
	UIRequest uiReq;
	
	if ( sw < 0 || sw >= NUM_SWTS ) return INVALID_SWITCH;
	
	if ( dir == SWITCH_STRAIGHT || dir == SWITCH_CURVED ) {
		// TODO wait?
		char bytes[3] = { 33 + dir, (char) sw, 32 };// 32 TURNS OFF SOLENOID

		// Send the command
		ret = PutStr( bytes, sizeof(bytes), ts->iosTid );
		
		// Store the new direction
		ts->switches[sw] = dir;    
		
		// Update the UI
		uiReq.type  = TRACK_SERVER;
		uiReq.idx   = sw;
		uiReq.state = (int)dir;
		Send( ts->uiTid, (char*) &uiReq, sizeof(UIRequest),
		  			 (char*) &sw, sizeof(int) );
	} else {
		ret = INVALID_DIR;
		error( ret, "TrackServer received invalid direction.");
	}
	return ret;
}

// set all the switches to given direction
int ts_switchSetAll( TS *ts, SwitchDir *dirLow, SwitchDir *dirHigh ) {
    int err = 0;

	int i;
    for( i = 1; i <= 18; i ++ ) {
		err |= ts_switchSet( ts, i, dirLow[i] );
	}
    for( i = 153; i <= 156; i ++ ) {
		err |= ts_switchSet( ts, i, dirHigh[i-153] );
	}

	return ( err < NO_ERROR ) ? CANNOT_INIT_SWITCHES : NO_ERROR;
}

