/*
 * CS 452: Train Controller User Task
 */
#define DEBUG 1
#define POLL_INTERVAL 5
#define POLL_SIZE 10

#include <string.h>
#include <ts7200.h>

#include "debug.h"
#include "error.h"
#include "math.h"
#include "requests.h"
#include "servers.h"
#include "shell.h"
#include "string.h"
#include "task.h"
#include "traincontroller.h"

/* FORWARD DECLARATIONS */

/**
 * A Train Controller
 */
typedef struct {
    char lstSensorCh;
    int  lstSensorNum;
    char speeds   [NUM_TRNS];
    char switches [NUM_SWTS];
	TID  iosTid;
	TID  csTid;
} TC;

int tc_init			( TC *tc );
int tc_trainSet		( TC *tc, int train, int speed );
int tc_switchSet	( TC *tc, int sw, int dir );
int tc_switchSetAll	( TC *tc, char dir );
void tc_pollSensors	( TC *tc );
int checkTrain		( int train );
int checkDir		( int *dir );
void poll			();

/* ACTUAL CODE */

void tc_run () {
	debug ("tc_run: The Train Controller is about to start. \r\n");	
	TC tc;
	int 	   senderTid;
	TCRequest  req;
	int 	   ret, len;
	int 	   speed;

	// Initialize the Train Controller
	if_error( tc_init (&tc), "Initializing Train Controller failed.");
	 

	FOREVER {
		// Receive a server request
		debug ("tc: is about to Receive. \r\n");
		len = Receive ( &senderTid, (char *) &req, sizeof(TCRequest) );
		debug ("tc: Received: fromTid=%d type=%d arg1=%d arg2=%d len=%d\r\n", 
				senderTid, req.type, req.arg1, req.arg2, len);
	
		assert( len == sizeof(TCRequest) );
		
		switch (req.type) {
			case RV:
				debug ("tc: Reversing %d.\r\n", req.train);
				
				if ( (ret = checkTrain ( req.train )) >= NO_ERROR ) {
					speed = tc.speeds[req.train];
					
					ret =  tc_trainSet( &tc, req.train, 0 );
					ret |= tc_trainSet( &tc, req.train, 15 );
					ret |= tc_trainSet( &tc, req.train, speed );
				}
				
				break;
			case ST:
				debug ("Switch %d is set to %c.\r\n", req.sw, tc.switches[req.sw]);
				ret = tc.switches[req.sw]; 
				break;

			case SW:
				debug ("tc: Setting switch %d to dir %c.\r\n", req.sw, (char)req.arg2);
				ret = tc_switchSet( &tc, req.sw, req.dir );
				break;
			
			case TR:
				debug("tc: Setting train #%d to speed %d.\r\n", req.sw, req.arg2 );
				ret = tc_trainSet( &tc, req.train, req.speed );
				break;

			case WH:
				debug ("tc: WHing\r\n");
				// TODO this should not use bw or print out for that matter
				if( tc.lstSensorCh == 0 ) {
					bwputstr( COM2, "No sensor triggered yet\r\n" );
				} else {
					bwprintf( COM2, "Last sensor triggered was %c%d.\r\n", 
							  tc.lstSensorCh, tc.lstSensorNum );
				}
				ret = tc.lstSensorNum;
				break;

			case POLL:
				debug("tc: Poll results  %c%d \r\n", req.channel, req.sensor);
				bwprintf( COM2, "%c%d\r\n", req.channel, req.sensor );
				tc.lstSensorCh = req.channel;
				tc.lstSensorNum = req.sensor;
				break;
			
			default:
				ret = TC_INVALID_REQ_TYPE;
				error (ret, "Train Controller request type is not valid.");
				break;
		}

		debug ("tc: about to Reply to %d. \r\n", senderTid);
		Reply ( senderTid, (char *) &ret, sizeof(ret) );
		debug ("tc: returned from Replying to %d. \r\n", senderTid);

	}
}

int tc_init (TC *tc) {
	debug ("tc_init: train controller=%x \r\n", tc);

	tc->lstSensorCh  = 0;
	tc->lstSensorNum = 0;
	tc->csTid = WhoIs( CLOCK_NAME );
	tc->iosTid = WhoIs( SERIALIO1_NAME );

	memoryset ( tc->speeds, 0, NUM_TRNS );
	tc_switchSetAll( tc, 'S' );

	// Turn on the train box
	Putc( 96, tc->iosTid );
	
	return Create( 3, &poll ) & RegisterAs( TRAIN_CONTROLLER_NAME );
}

// check if the train index is within range
int checkTrain( int train ) {
    if (train >= 0 && train < NUM_TRNS) {
		return NO_ERROR;
	}

	debug ("tc: Invalid train value %d.\r\n", train);
	return INVALID_TRAIN; 
}

// check if the direction index is within range
int checkDir( int *dir ) {
    if( *dir == 'c' || *dir == 'C' ) { *dir = 34; return NO_ERROR; }
    if( *dir == 's' || *dir == 'S' ) { *dir = 33; return NO_ERROR; }
    
	debug("tc: Train direction %c not in range.\r\n", dir );
    return INVALID_DIR;
}

// send commands to the train, try a few times
int tc_trainSet( TC *tc, int train, int speed ) {
    debug ("tc: trainSet: tr=%d sp=%d\r\n", train, speed);

	// check the train number is valid
	int err = checkTrain( train );
	if( err >= NO_ERROR ) {

		char bytes[2] = { (char) speed, (char) train };
		int  i;

		for( i = 0; i < TRAIN_TRIES; i ++ ) {
			if((err=PutStr( bytes, sizeof(bytes), tc->iosTid)) >= NO_ERROR ) {
				debug ("tc: trainSend was successful on try %d\r\n", i);
				break;
			}
			error(err, "Sending to train cnotroller failed.\r\n");
		}

		if( i == TRAIN_TRIES ) {
			debug("Sending to train: Connection timeout.\r\n" );
			return CONNECTION_TIMEOUT;
		}

		// Store the new train speed
		tc->speeds[train] = (char) speed;
	}
    return err;
}

// set a switch to a desired position checking for bad input
int tc_switchSet( TC *tc, int sw, int dir ) {
    debug ("tc: switchSet: sw=%d dir=%c\r\n", sw, dir );

	// check the direction is a valid character
	int err = checkDir( &dir );
	if ( err >= NO_ERROR ) {
		char bytes[3] = { (char) dir, (char) sw, 32 };// 32 TURNS OFF SOLENOID
		// TODO: wait?

		// Send the command
		err = PutStr( bytes, sizeof(bytes), tc->iosTid );
		
		// Store the new direction
		tc->switches[sw] = (char) dir;    
	}
	return err;
}

// set all the switches to given direction
int tc_switchSetAll( TC *tc, char dir ) {
    int err = 0;

	int i;
    for( i = 1; i <= 18; i ++ ) {
		err |= tc_switchSet( tc, i, dir );
	}
    for( i = 153; i <= 156; i ++ ) {
		err |= tc_switchSet( tc, i, dir );
	}

	return ( err < NO_ERROR ) ? CANNOT_INIT_SWITCHES : NO_ERROR;
}

void poll() {
	debug( "tc: polling task started\r\n" );

	TID tcTid = MyParentTid();
	TID csTid = WhoIs( CLOCK_NAME );
	TID ioTid = WhoIs( SERIALIO1_NAME );
	char poll_req[2] = { 133, 192 }, ch;
	// We need the braces around the 0s because they're in unions
	TCRequest req = { POLL, {0}, {0} };
	int i, res;

	FOREVER {

		// Only updated ever so often
		Delay( POLL_INTERVAL, csTid );

		// Poll the train box
		PutStr( poll_req, 1, ioTid );
		for( i = 0; i < POLL_SIZE; i++ ) {
			res = Getc( ioTid );
			//TODO remove next line ?
			if_error( res, "Polling train box failed" );
			ch = (char) res;
			// See if a sensor was flipped
			if( res >= NO_ERROR && ch ) { 
				req.channel = 'A' + (i / 2);
				req.sensor = rev_log2( ch );
				if( i % 2 ) req.sensor += 8;
			}
		}

		// Let the train controller know
		Send( tcTid, (char *) &req, sizeof(req), (char*) &res, sizeof(res));
	}

}
