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
} TS;

int ts_start		( TS *ts );
int ts_stop			( TS *ts );
int ts_init			( TS *ts );
int ts_trainSet		( TS *ts, int train, int speed );
int ts_switchSet	( TS *ts, int sw, char dir );
int ts_switchSetAll	( TS *ts, char dir );
int checkTrain		( int train );
SwitchDir checkDir	( int dir );

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
	
		assert( len == sizeof(TSRequest) );
		
		switch (req.type) {
			case RV:
				debug ("ts: Reversing %d.\r\n", req.train);
				
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
				reply.dir = (ts.switches[req.sw]) ? 'C' : 'S'; 
				debug ("ts: Switch %d is set to %c.\r\n", req.sw, reply.dir);
				break;

			case SW:
				debug ("ts: Setting switch %d to dir %c.\r\n", req.sw, req.dir);
				reply.ret = ts_switchSet( &ts, req.sw, req.dir );
				break;
			
			case TR:
				debug ("ts: Setting train #%d to speed %d.\r\n", req.sw, req.speed );
				reply.ret = ts_trainSet( &ts, req.train, req.speed );
				break;

			case WH:
				debug ("ts: WHing\r\n");
				reply.sensor = ts.lstSensor;
				reply.ticks = (Time( ts.csTid ) - ts.lstSensorPoll);
				break;

			case POLL:
				debug("ts: Poll results  %c%d \r\n", req.channel, req.sensor);
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
	int err = NO_ERROR;

	ts->lstSensor  = -1;
	ts->lstSensorUpdate = 0;
	ts->csTid = WhoIs( CLOCK_NAME );
	ts->iosTid = WhoIs( SERIALIO1_NAME );

	ts_start( ts );
	
	memoryset ( ts->speeds, 0, NUM_TRNS );
	ts_switchSetAll( ts, 'C' );
	
	err = Create( 3, &det_run );
	if( err < NO_ERROR ) return err;
	err = RegisterAs( TRACK_SERVER_NAME );
	return err;
}

int ts_start( TS *ts ) {
	return Putc( 96, ts->iosTid );
}

int ts_stop( TS *ts ) {
	return Putc( 97, ts->iosTid );
}

// check if the train index is within range
int checkTrain( int train ) {
    if (train >= 0 && train < NUM_TRNS) {
		return NO_ERROR;
	}

	debug ("ts: Invalid train value %d.\r\n", train);
	return INVALID_TRAIN; 
}

// check if the direction index is within range
SwitchDir checkDir( int dir ) {
    if( dir == 's' || dir == 'S' ) { return SWITCH_STRAIGHT; }
    if( dir == 'c' || dir == 'C' ) { return SWITCH_CURVED; }
    
	debug("ts: Train direction %c not in range.\r\n", dir );
    return INVALID_DIR;
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
int ts_switchSet( TS *ts, int sw, char dir ) {
    debug ("ts: switchSet: sw=%d dir=%c\r\n", sw, dir );
	// check the direction is a valid character
	int ret = checkDir( dir );
	if ( ret >= NO_ERROR ) {
		SwitchDir swd = ret;
		// TODO wait?
		char bytes[3] = { 33 + ret, (char) sw, 32 };// 32 TURNS OFF SOLENOID

		// Send the command
		ret = PutStr( bytes, sizeof(bytes), ts->iosTid );
		
		// Store the new direction
		ts->switches[sw] = swd;    
	}
	return ret;
}

// set all the switches to given direction
int ts_switchSetAll( TS *ts, char dir ) {
    int err = 0;

	int i;
    for( i = 1; i <= 18; i ++ ) {
		err |= ts_switchSet( ts, i, dir );
	}
    for( i = 153; i <= 156; i ++ ) {
		err |= ts_switchSet( ts, i, dir );
	}

	return ( err < NO_ERROR ) ? CANNOT_INIT_SWITCHES : NO_ERROR;
}

// format sensor id into human readable output
char sensor_bank( int sensor ) {
	return 'A' + (sensor / SIZE_BANK);
}
char sensor_num( int sensor ) {
	return (sensor % SIZE_BANK) + 1;
}

