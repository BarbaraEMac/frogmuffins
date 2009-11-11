/*
 * CS 452: Track Server User Task
 */
#define DEBUG 		1

#define POLL_SIZE 	10
#define NUM_TRNS    81
#define NUM_SWTS    256
#define SW_CNT      22
#define SW_WAIT     2	// ticks
#define SNSR_WAIT   1	// ticks
#define TRAIN_WAIT	10	// ticks
#define POLL_WAIT 	(100 / MS_IN_TICK)
#define POLL_GRACE  (5000 / MS_IN_TICK) // wait 5 seconds before complaining
#define POLL_CHAR	133

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

/* FORWARD DECLARATIONS */

/**
 * A Track Server
 */
typedef struct {
    char 		lstSensorId;
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
void ts_pollSensors	( TS *ts );
int checkTrain		( int train );
SwitchDir checkDir	( int dir );
void poll			();
void pollWatchDog	();

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
				reply.channel = 'A' + (ts.lstSensorId / SIZE_BANK);
				reply.sensor = (ts.lstSensorId % SIZE_BANK) + 1;
				reply.ticks = (Time( ts.csTid ) - ts.lstSensorPoll);
				break;

			case POLL:
				debug("ts: Poll results  %c%d \r\n", req.channel, req.sensor);
				if( req.sensorId != ts.lstSensorId ) {
					ts.lstSensorUpdate = req.ticks;
				}
				ts.lstSensorId = req.sensorId;
				ts.lstSensorPoll = req.ticks;
				break;

			case WATCH_DOG:
				debug("ts: Watchdog stamp %d ticks\r\n", req.ticks);
				if( ts.lstSensorPoll < req.ticks ) {
					error( TIMEOUT, "ts: Polling timed out. Retrying." );
					// Don't tell me for another 5 seconds
					ts.lstSensorPoll = req.ticks + POLL_GRACE;
					Putc( POLL_CHAR, ts.iosTid );
				}
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

	ts->lstSensorId  = 0;
	ts->lstSensorPoll = POLL_GRACE; //wait 5 seconds before complaining
	ts->lstSensorUpdate = 0;
	ts->csTid = WhoIs( CLOCK_NAME );
	ts->iosTid = WhoIs( SERIALIO1_NAME );

	ts_start( ts );
	
	memoryset ( ts->speeds, 0, NUM_TRNS );
	ts_switchSetAll( ts, 'C' );
	
	err = Create( 3, &det_run );
	if( err < NO_ERROR ) return err;
	err =  Create( 3, &poll );
	if( err < NO_ERROR ) return err;
	err = Create( 3, &pollWatchDog );
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

void poll() {
	debug( "ts: polling task started\r\n" );

	TID tsTid = MyParentTid();
	TID csTid = WhoIs( CLOCK_NAME );
	TID ioTid = WhoIs( SERIALIO1_NAME );
	TID detTid = WhoIs( TRACK_DETECTIVE_NAME );
	char ch;
	// We need the braces around the 0s because they're in unions
	TSRequest 	req = { POLL, {0}, {0} };
	DetRequest	req2;
	TSReply		rpl;
	int 		i, res;

	req2.type = POLL;

	FOREVER {

		// Only updated ever so often
		//Delay( SNSR_WAIT, csTid );

		// Poll the train box
		Putc( POLL_CHAR, ioTid );
		for( i = 0; i < POLL_SIZE; i++ ) {
			res = Getc( ioTid );
			if( res < NO_ERROR ) {
				error( res, "ts: Polling train box failed" );
				break;
			}
			ch = (char) res;
			req2.rawSensors[i] = ch;
			// See if a sensor was flipped
			if( res >= NO_ERROR && ch ) { 
				req.sensorId = (i * 8) + (7 - log_2( ch ));
			}
		}
		req.ticks = Time( csTid );

		// Let the track server know
		Send( tsTid, (char *) &req, sizeof(req), (char*) &rpl, sizeof(rpl));
	}

}

void pollWatchDog () {

	debug( "ts: poll watshdog task started\r\n" );

	TID tsTid = MyParentTid();
	TID csTid = WhoIs( CLOCK_NAME );
	// We need the braces around the 0s because they're in unions
	TSRequest 	req = { WATCH_DOG, {0}, {0} };
	TSReply		rpl;

	FOREVER {
		// Watch dog timeout ever so often
		req.ticks = Time( csTid );
		Delay( POLL_WAIT, csTid );

		// Let the track server know
		Send( tsTid, (char *) &req, sizeof(req), (char*) &rpl, sizeof(rpl));
	}


}
