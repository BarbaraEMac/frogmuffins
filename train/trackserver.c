/*
 * CS 452: Train Controller User Task
 */
#define DEBUG 		1

#define POLL_SIZE 	10
#define NUM_TRNS    81
#define NUM_SWTS    256
#define SW_CNT      22
#define SW_WAIT     2	// ticks
#define SNSR_WAIT   1	// ticks
#define TRAIN_WAIT	10	// ticks
#define POLL_WAIT 	(1000/ MS_IN_TICK)
#define POLL_CHAR	133

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
#include "train.h"
#include "model.h"

/* FORWARD DECLARATIONS */

/**
 * A Train Controller
 */
typedef struct {
    char lstSensorCh;
    char lstSensorNum;
	int  lstSensorUpdate; // int ticks
    char speeds   [NUM_TRNS];
    char switches [NUM_SWTS];
	TID  iosTid;
	TID  csTid;
	TrackModel model;
} TC;

int tc_start		( TC *tc );
int tc_stop			( TC *tc );
int tc_init			( TC *tc );
int tc_trainSet		( TC *tc, int train, int speed );
int tc_switchSet	( TC *tc, int sw, char dir );
int tc_switchSetAll	( TC *tc, char dir );
void tc_pollSensors	( TC *tc );
int checkTrain		( int train );
int checkDir		( int dir );
void poll			();
void pollWatchDog	();

/* ACTUAL CODE */

void tc_run () {
	debug ("tc_run: The Train Controller is about to start. \r\n");	
	TC 			tc;
	int			senderTid;
	TCRequest 	req;
	TCReply		reply;
	int			speed, len;

	// Initialize the Train Controller
	if_error( tc_init (&tc), "Initializing Train Controller failed.");

	FOREVER {
		// Receive a server request
		debug ("tc: is about to Receive. \r\n");
		len = Receive ( &senderTid, (char *) &req, sizeof(req) );
		debug ("tc: Received: fromTid=%d type=%d arg1=%d arg2=%d len=%d\r\n", 
				senderTid, req.type, req.arg1, req.arg2, len);
	
		assert( len == sizeof(TCRequest) );
		
		switch (req.type) {
			case RV:
				debug ("tc: Reversing %d.\r\n", req.train);
				
				if ( (reply.ret = checkTrain ( req.train )) >= NO_ERROR ) {
					speed = tc.speeds[req.train];
					
					reply.ret =  tc_trainSet( &tc, req.train, 0 );
					// Wait for slow down depending on previous speed 
					// TODO is the server allowed to block like this?
					Delay( speed, tc.csTid );
					reply.ret |= tc_trainSet( &tc, req.train, 15 );
					reply.ret |= tc_trainSet( &tc, req.train, speed );
				}
				
				break;
			case ST:
				reply.dir = tc.switches[req.sw]; 
				debug ("tc: Switch %d is set to %c.\r\n", req.sw, reply.dir);
				break;

			case SW:
				debug ("tc: Setting switch %d to dir %c.\r\n", req.sw, req.dir);
				reply.ret = tc_switchSet( &tc, req.sw, req.dir );
				break;
			
			case TR:
				debug ("tc: Setting train #%d to speed %d.\r\n", req.sw, req.speed );
				reply.ret = tc_trainSet( &tc, req.train, req.speed );
				break;

			case WH:
				debug ("tc: WHing\r\n");
				reply.sensor = tc.lstSensorNum;
				reply.channel = tc.lstSensorCh;
				reply.ticks = (Time( tc.csTid ) - tc.lstSensorUpdate);
				break;

			case POLL:
				debug("tc: Poll results  %c%d \r\n", req.channel, req.sensor);
				tc.lstSensorCh = req.channel;
				tc.lstSensorNum = req.sensor;
				tc.lstSensorUpdate = Time( tc.csTid );
				break;

			case WATCH_DOG:
				debug("tc: Watchdog stamp %d ticks\r\n", req.timeStamp );
				if( tc.lstSensorUpdate < req.timeStamp ) {
					error( TIMEOUT, "tc: Polling timed out. Retrying." );
					Putc( POLL_CHAR, tc.iosTid );
				}
				break;

			case START:
				tc_start( &tc );
				break;

			case STOP:
				tc_stop( &tc );
				break;
			
			default:
				reply.ret = TC_INVALID_REQ_TYPE;
				error (reply.ret, "Train Controller request type is not valid.");
				break;
		}

		debug ("tc: about to Reply to %d. \r\n", senderTid);
		Reply ( senderTid, (char *) &reply, sizeof(reply) );
		debug ("tc: returned from Replying to %d. \r\n", senderTid);

	}
}

int tc_init( TC *tc ) {
	debug ("tc_init: train controller=%x \r\n", tc);
	int err = NO_ERROR;

	tc->lstSensorCh  = 0;
	tc->lstSensorNum = 0;
	tc->csTid = WhoIs( CLOCK_NAME );
	tc->iosTid = WhoIs( SERIALIO1_NAME );

	tc_start( tc );
	
	memoryset ( tc->speeds, 0, NUM_TRNS );
	tc_switchSetAll( tc, 'C' );
	
	parse_model( TRACK_A, &tc->model ); 

	err =  Create( 3, &poll );
	if( err < NO_ERROR ) return err;
	err = Create( 3, &pollWatchDog )
	if( err < NO_ERROR ) return err;
	err = RegisterAs( TRAIN_CONTROLLER_NAME );
	return err;
}

int tc_start( TC *tc ) {
	return Putc( 96, tc->iosTid );
}

int tc_stop( TC *tc ) {
	return Putc( 97, tc->iosTid );
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
int checkDir( int dir ) {
    if( dir == 'c' || dir == 'C' ) { return 34; }
    if( dir == 's' || dir == 'S' ) { return 33; }
    
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

		// Send the command
		err = PutStr( bytes, sizeof(bytes), tc->iosTid);
		if_error( err, "Sending to train controller failed." );

		// Store the new train speed
		tc->speeds[train] = (char) speed;
	}
    return err;
}

// set a switch to a desired position checking for bad input
int tc_switchSet( TC *tc, int sw, char dir ) {
    debug ("tc: switchSet: sw=%d dir=%c\r\n", sw, dir );

	// check the direction is a valid character
	int ret = checkDir( dir );
	if ( ret >= NO_ERROR ) {
		// TODO wait?
		char bytes[3] = { (char) ret, (char) sw, 32 };// 32 TURNS OFF SOLENOID

		// Send the command
		ret = PutStr( bytes, sizeof(bytes), tc->iosTid );
		
		// Store the new direction
		tc->switches[sw] = dir;    
	}
	return ret;
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
	char ch;
	// We need the braces around the 0s because they're in unions
	TCRequest 	req = { POLL, {0}, {0} };
	TCReply		rpl;
	int 		i, res;

	FOREVER {

		// Only updated ever so often
		Delay( SNSR_WAIT, csTid );

		// Poll the train box
		Putc( POLL_CHAR, ioTid );
		for( i = 0; i < POLL_SIZE; i++ ) {
			res = Getc( ioTid );
			if( res < NO_ERROR ) {
				error( res, "tc: Polling train box failed" );
				break;
			}
			ch = (char) res;
			// See if a sensor was flipped
			if( res >= NO_ERROR && ch ) { 
				req.channel = 'A' + (i / 2);
				req.sensor = rev_log2( ch );
				if( i % 2 ) req.sensor += 8;
			}
		}

		// Let the train controller know
		Send( tcTid, (char *) &req, sizeof(req), (char*) &rpl, sizeof(rpl));
	}

}

void pollWatchDog () {

	debug( "tc: poll watchdog task started\r\n" );

	TID tcTid = MyParentTid();
	TID csTid = WhoIs( CLOCK_NAME );
	// We need the braces around the 0s because they're in unions
	TCRequest 	req = { WATCH_DOG, {0}, {0} };
	TCReply		rpl;

	FOREVER {
		// Watch dog timeout ever so often
		req.timeStamp =  Time( csTid );
		Delay( POLL_WAIT, csTid );

		// Let the train controller know
		Send( tcTid, (char *) &req, sizeof(req), (char*) &rpl, sizeof(rpl));
	}


}
/*
void getcHelper() {
	helperMsg 	msg;
	TID			parent;
	TID			iosTid = WhoIs( SERIALIO1_NAME );
	// Get instructions on what to do
	Receive( &parent, (char *) &msg, sizeof(helperMsg) );
	Reply( parent, 0 , 0 );
	// Get a character from the IO server
	msg.result = Getc( iosTid );
	// Return result back to parent
	Send( parent, (char*) &msg, sizeof(helperMsg), 0, 0);
}

void delayHelper() {
	helperMsg 	msg;
	TID			parent;
	TID			csTid = WhoIs( CLOCK_NAME );
	// Get instructions on what to do
	Receive( &parent, (char *) &msg, sizeof(helperMsg) );
	Reply( parent, 0 , 0 );
	// Wait for specified number of ticks
	msg.result = Delay( msg.ticks, csTid );
	// Return result back to parent
	Send( parent, (char*) &msg, sizeof(helperMsg), 0, 0);
}

	
timeoutPoll( TID delay, TID getc, int ticks ) {
	helperMsg 	msg;
	TID 		getcTid = Create(3, &getcHelper );
	TID 		delayTid = Create(3, &delayHelper );
	TID			tid;
	// Start off the getc helper
	msg.tid = iosTid;
	Send( getcTid, (char *) &msg, sizeof(helperMsg), 0, 0 );
	// Start off the delay helper
	msg.tid = csTid;
	msg.ticks = timeout;
	Send( delayTid, (char *) &msg, sizeof(helperMsg), 0, 0 );
	// Wait for one of them to finish
	Receive( &tid, (char *) &msg, sizeof(helperMsg) );
	// Destroy them
	if( msg.tid = delayTid ) return TIMEOUT;
	else return msg.result;
}
*/
