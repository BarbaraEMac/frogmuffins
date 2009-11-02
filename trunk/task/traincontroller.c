/*
 * CS 452: Train Controller User Task
 */
#define DEBUG 2

#include <bwio.h>
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

void tc_run () {
	debug ("tc_run: The Train Controller is about to start. \r\n");	
	TrainController tc;
	int 	   senderTid;
	TCRequest  req;
	int 	   ret, len;
	TID		   ios1Tid = WhoIs(SERIALIO1_NAME);
	TID		   ios2Tid = WhoIs(SERIALIO2_NAME);
	TID		   csTid  = WhoIs(CLOCK_NAME);
	int 	   speed;

	// Initialize the Train Controller
	tc_init (&tc);

	FOREVER {
		// Receive a server request
		debug ("tc: is about to Receive. \r\n");
		len = Receive ( &senderTid, (char *) &req, sizeof(TCRequest) );
		debug ("tc: Received: fromTid=%d type=%d arg1=%d arg2=%d len=%d\r\n", 
				senderTid, req.type, req.arg1, req.arg2, len);
	
		assert( len == sizeof(TCRequest) );
		
		debug ("tc: about to Reply to %d. \r\n", senderTid);
		Reply ( senderTid, (char *) &ret, sizeof(int) );
		debug ("tc: returned from Replying to %d. \r\n", senderTid);

		// TODO: Poll the sensors

		switch (req.type) {
			case RV:
				debug ("tc: Reversing %d.\r\n", req.arg1);
				
				if ( (ret = checkTrain ( req.arg1 )) >= NO_ERROR ) {
					speed = tc.speeds[req.arg1];
					
					ret =  trainSend ((char) 0,     (char) req.arg1, ios1Tid, csTid);
					ret |= trainSend ((char) 15,    (char) req.arg1, ios1Tid, csTid);
					ret |= trainSend ((char) speed, (char) req.arg1, ios1Tid, csTid);
				}
				
				break;
			case ST:
				debug ("Switch %d is set to %c.\r\n", req.arg1, tc.switches[req.arg1]);

				break;

			case SW:
				debug ("tc: Setting switch %d to dir %c.\r\n", req.arg1, (char)req.arg2);
  
				if ( (ret = checkDir ( &req.arg2 )) >= NO_ERROR ) {
					ret = switchSend( (char) req.arg1, (char) req.arg2, tc.switches, ios1Tid );
				}
				break;
			
			case TR:
				debug("tc: Setting train #%d to speed %d.\r\n", req.arg1, req.arg2 );
	
				if( (ret = checkTrain ( req.arg1 )) >= NO_ERROR ) {
					ret = trainSend( (char) req.arg2, (char) req.arg1, ios1Tid, csTid);
					
					// Store the new train speed
					tc.speeds[req.arg1] = (char) req.arg2;
				}
		
				break;

			case WH:
				debug ("tc: WHing\r\n");
				if( tc.lstSensorCh == 0 ) {
					bwputstr( COM2, "No sensor triggered yet\r\n" );
				}
        		else {
					bwprintf( COM2, "Last sensor triggered was %c%d.\r\n", 
							  tc.lstSensorCh, tc.lstSensorNum );
				}

				break;
			
			default:
				ret = TC_INVALID_REQ_TYPE;
				error (ret, "Train Controller request type is not valid.");
				break;
		}
	}
}

int tc_init (TrainController *tc) {
	debug ("tc_init: train controller=%x \r\n", tc);

	tc->lstSensorCh  = 0;
	tc->lstSensorNum = 0;

	memoryset ( tc->speeds, 0, NUM_TRNS );
	switches_init ( 'S', tc->switches, ios1Tid );
	
	return RegisterAs(TRAIN_CONTROLLER_NAME);
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
int trainSend( char byte1, char byte2, TID ios1Tid, TID csTid ) {
    debug ("tc: trainSend: b1=%d b2=%d\r\n", byte1, byte2);
	char bytes[2];
	bytes[0] = byte1;
	bytes[1] = byte2;
	int err;

	int i;
    for( i = 0; i < TRAIN_TRIES; i ++ ) {
        if((err=PutStr( bytes, 2, ios1Tid)) >= NO_ERROR ) {
			debug ("tc: trainSend was successful on try %d\r\n", i);
        	break;
		}
		error(err, "Sending to train cnotroller failed.\r\n");
    }

    if( i == TRAIN_TRIES ) {
        debug("Sending to train: Connection timeout.\r\n" );
        return CONNECTION_TIMEOUT;
    }

    return NO_ERROR;
}

// set a switch to a desired position checking for bad input
int switchSend( char sw, char dir, char* switches, TID ios1Tid ) {
	int err;
	char bytes[3] = { dir, sw, 32 };	// 32 = TURN OFF THE SOLENOID
	
	// TODO: wait?

	err = PutStr( bytes, 3, ios1Tid );
		
	// Store the new direction
	switches[(int) sw] = dir;    
	
	return err;
}

// set all the switches to given direction
int switches_init( char dir, char* switches, TID ios1Tid ) {
    int err = 0;

	int i;
    for( i = 1; i <= 18; i ++ ) {
		err |= switchSend( i, dir, switches, ios1Tid );
	}
    for( i = 153; i <= 156; i ++ ) {
		err |= switchSend( i, dir, switches, ios1Tid );
	}

	return ( err < NO_ERROR ) ? CANNOT_INIT_SWITCHES : NO_ERROR;
}

void pollSensors( TrainController *tc, TID ios1Tid ) {
    int err;
    char ch=0; int i=-1, res;//, old_time=*(tc.clock);

	// TODO:Remove this!
	bwclear( COM1 );                // clear the io before reading
    
	err = Putc( 133, ios1Tid );
    err = Putc( 192, ios1Tid );   // ask for sensor dump
    
	// look for the first non-empty char
    while( (++ i < 10) && (err = Getc(ios1Tid) < NO_ERROR) && !ch ) {
		ch = err;
	}

    if( err < NO_ERROR  ) {
		debug( "Reading: Connection Timeout" );
	}

    if( ch && err ) {
        tc->lstSensorCh = 'A' + (i / 2);
        tc->lstSensorNum = rev_log2( ch );
        if( i % 2 ) tc->lstSensorNum +=8;
    }
    
	// TODO:Remove this!
	bwclear( COM1 );                // ignore the rest of the input
}
