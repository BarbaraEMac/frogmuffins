/*
 * Shell for Kernel 
 * becmacdo
 * dgoc
 */
#define DEBUG 1

#include <string.h>
#include <ts7200.h>

#include "debug.h"
#include "globals.h"
#include "model.h"
#include "requests.h"
#include "routeplanner.h"
#include "servers.h"
#include "shell.h"
#include "task.h"
#include "trackserver.h"
#include "train.h"
#include "ui.h"

#define INPUT_LEN   60
#define INPUT_HIST  1
#define IO			5	// harcoded in this file as it actually creates it

#define output(args...)	cprintf( IO, "\033[36m" ); \
						cprintf( IO, args ); \
						cprintf( IO, "\033[37m" )

// Private Stuff
// ----------------------------------------------------------------------------
typedef struct {
	TID ns;
	TID cs;
	TID ios1;
	TID ios2;
	TID ts;
	TID rp;
	TID ui;
	TID tr[2];	
	TID idle;
} TIDs;

// Use this function to grab a line of data before the shell starts.
//void shell_inputData 	( char *input, bool reset );

void shell_initTrack	( TIDs *tids );
void shell_cmdTrain 	( TIDs *tids, const char *dest, int id, TrainMode mode );
void shell_initTrain 	( TIDs *tids, int trainNum );

/**
 * Given an input command, error check it and execute it.
 */
void shell_exec 		( TIDs *tids, char *command );
void shell_run	 		( TIDs *tids );

RPShellReply rpCmd		( RPRequest *req, TID rpTid );

// ----------------------------------------------------------------------------

// A fun idle task that counts to high numbers
void idleTask(  ) {
	debug( "idleTask running\r\n" );
	int i = 0;
	RegisterAs( "Idle" );

	while( 1 ) {
		//if( ( i % 20 )== 0 ) {
		//	printf( COM2, "IDLE=%d\r\n", i );
		//}
		i = i + 1;
	}
}

void bootstrap(  ) {
	TIDs tids;

	// Create the idle task
	tids.idle = Create( IDLE_PRTY, &idleTask );

	// Create the name server
	// Create the Serial I/O server
	tids.ns   = Create( HW_SERVER_PRTY, &ns_run );
	tids.ios1 = Create( HW_SERVER_PRTY, &ios1_run );
    tids.ios2 = Create( HW_SERVER_PRTY, &ios2_run );
	assert( IO == tids.ios2 );
//	output( "Running on board %08x. \r\n", board_id(  ) );
//	output( "Initializing the name and serial io servers. \r\n" );

	// Create the clock server
	tids.cs = Create( HW_SERVER_PRTY, &cs_run );
//	output( "Initializing the clock server. \r\n" );
	
	// Create the routeplanner
	tids.rp = Create( LOW_SERVER_PRTY, &rp_run );
//	output( "Initializing the route planner. \r\n" );

	// Create the ui 
	// Makes 1 clock notifying task
	// TID == 11, 12
	tids.ui = Create( OTH_SERVER_PRTY, &ui_run );
	output( "Initializing the UI (tid=%d). \r\n", tids.ui );
	
	// Initialize the track we want to use.
	shell_initTrack( &tids );

	// Create the train controller
	// TID = 13, 14
	tids.ts = Create( OTH_SERVER_PRTY, &ts_run );
//	output( "Initializing the track server (tid=%d). \r\n", tids.ts );
	
	// Create the first train!
	tids.tr[0] = Create( TRAIN_PRTY, &train_run );
//	output( "Creating the first train! (%d)\r\n", tids.tr[0] );
	
	// Create the second train!
	tids.tr[1] = Create( TRAIN_PRTY, &train_run );
//	output( "Creating the first train!\r\n" );

	// Initialize the first train
	shell_initTrain( &tids, 0 /*train num*/);
	
	// Initialize the second train
	shell_initTrain( &tids, 1 /*train num*/ );

	// Run the shell
	shell_run( &tids );
}

void shell_run( TIDs *tids ) {
    int   h = 0;
    char *input, history[INPUT_HIST][INPUT_LEN];
	
	input    = history[h++];
	input[0] = '\0';

//	output( "\r\nType 'h' for a list of commands.\r\n" );
	output ("\r> ");

	FOREVER {
		shell_inputData( input, true );
		shell_exec( tids, input );
	
		/*input = history[h++];
		h %= INPUT_HIST;*/
    }
}

void shell_inputData( char *input, bool reset ) {
	char ch;
	int i = 0;
	if( reset ) {
		input[0] = '\0';
	}
	
	// Print out the default value
	while( input[i] ) {
		output( "%c", input[i++] );
	}

	FOREVER {
        input[i] = 0;	// Clear the next character
		ch = Getc( IO );

		switch( ch ) {
			case '\r': // Enter was pressed
				output( "\r\n> " );
				return;
				break;
			case '\b': // Backspace was pressed
			case 127:
				if( i > 0 ) {
					output( "\b \b" );
					i --;
				}
				break;
			/*
			case '\033': // Escape sequence
				ch = Getc( IO );	// read the '['
				if( ch != '[' )break;
				ch = Getc( IO );
				output( "\r\n command %c detected.", ch );
				switch( ch ) {
					case 'A': // Up was pressed
					
						break;
					default:

						break;
				}
			*/
			default:	// Update the position in the command string
				output( "%c", ch );
				if( ch < 32 || ch > 126 ) {
					output( "%d", input[i] );
				}
				input[i++] = ch;
				break;
		}
        if( i == INPUT_LEN - 1 ) {	i-- ; output( "\b" );}
    }
}

void shell_initTrack( TIDs *tids ) {
	int 	err;
	char 	input[INPUT_LEN];
	
	if( board_id(  )== 0x9224e4a1 ) {
		input[0] = 'A';
	} else if( board_id(  )== 0x9224c1a8 ) {
		input[0] = 'B';
	} else {
		input[0] = 0;
	}
	input[1] = 0;

	output( "\033[24;1HTrack: " );
	shell_inputData( input, false );

	// Tell the UI which track we are using
	Send( tids->ui, &input[0], sizeof( char ), &err, sizeof( int ) );

	// Tell the Route Planner which track we are using
	Send( tids->rp, &input[0], sizeof( char ), &err, sizeof( int ) );
	
	if( err < NO_ERROR ) {
		output( "Invalid Track ID. Using Track B.\r\n" );
	}
}

void shell_cmdTrain( TIDs *tids, const char *dest, int i, TrainMode mode ) {
	assert( i >= 0 && i < array_size( tids->tr ) );
	
	TrainReq	 trReq;
	RPRequest	 rpReq;
	RPShellReply rpRpl;
	TID		 	 trainTid = tids->tr[i];

	// Parse the destination
	strncpy( rpReq.name, dest, 5 );
	rpReq.type = CONVERT_IDX;
	rpRpl = rpCmd( &rpReq, tids->rp );

	if( rpRpl.idx < NO_ERROR ) {
		output("Destination not found." );
		return;
	}

	trReq.type = DEST_UPDATE;
	trReq.mode = mode;
	trReq.dest = rpRpl.idx;

	// Tell the train its command info.
	Send( trainTid, &trReq, sizeof( TrainReq ), &trainTid, sizeof(int) );
}

void shell_initTrain( TIDs *tids, int i ) {
	TrainReq	req;
	char 		input[INPUT_LEN];
	
	assert( i >= 0 && i < array_size( tids->tr ) );
	TID			trainTid = tids->tr[i];
	req.type = TRAIN_INIT;

	FOREVER {
		output( "\033[24;1HTrain %d Id: ", i );
		shell_inputData( input, true );
		if( sscanf( input, "%d", &req.id ) >= 0 ) break;
		output( "Invalid train id. Try again.\r\n" );
	}

	// gear 7 is default
	input[0] = '7';
	input[1] = 0;

	FOREVER {
		output( "\033[24;1HTrain Gear: " );
		shell_inputData( input, false );
		// only take in valid train speeds
		if( sscanf( input, "%d", &req.gear ) >= 0
			&& req.gear > 0 && req.gear <= 14 ) break;
		output( "Invalid train gear. Try again.\r\n" );
	}

	// Tell the train its init info.
	Send( trainTid, &req, sizeof( TrainReq ), &trainTid, sizeof(int) );
	
	input[0] = '\0';
	
	// Send the train the stop distance
	req.type = STOP_UPDATE;
	FOREVER {
		output( "\033[24;1HStop Distance for train %d: ", req.id );
		shell_inputData( input, true );
		if( sscanf( input, "%d", &req.mm ) >= 0 ) break;
		output( "Invalid distance. Try again.\r\n" );
	}
	Send ( trainTid, &req, sizeof(TrainReq), 0, 0 );

	input[0] = '\0';
	
	req.type = DEST_UPDATE;
	FOREVER {
		output( "\033[24;1HParking Space: " );
		shell_inputData( input, false );
		// only take in valid train speeds
		if( sscanf( input, "%d", &req.dest ) >= 0 ) break;
	}
	shell_cmdTrain( tids, input, 0, DRIVE );
}

// Execute a train command
TSReply trackCmd( TSRequest *req, TIDs *tids ) {
	TSReply	rpl;
	req->startTicks = Time(tids->cs);
	
	Send( tids->ts, req, sizeof( TSRequest ), &rpl, sizeof( rpl ) ); 
	return rpl;
}

// Execute a Route Planner Command
RPShellReply rpCmd( RPRequest *req, TID rpTid ) {
	RPShellReply	rpl;
	
	Send( rpTid, req,  sizeof( RPRequest ), &rpl, sizeof( rpl ) ); 
	return rpl;
}

// Execute the command passed in
void shell_exec( TIDs *tids, char *command ) {
	char 			 tmpStr1[INPUT_LEN];
	char 		 	tmpStr2[INPUT_LEN];
	int			 	tmpInt;

	TSRequest		tsReq;
	TSReply			tsRpl;
	
	RPRequest		rpReq;
	RPShellReply	rpRpl;

	char *commands[] = {
		"\th = Help!", 
		"\tk1 = Execute kernel 1 user tasks.", 
		"\tk2 = Execute kernel 2 user tasks.", 
		"\tk3 = Execute kernel 3 user tasks.", 
	//	"cache ON/OFF = turn the instruction cache on/off respectively",
		"go = Initialize a train.",
		"++ Train Controller Commands ++",
		"\t rv train_num  = Reverse specified train.",
		"\t st switch_num = Display status of switch.",
		"\t sw switch_num dir = Switsh the switch in the sepcified direction.",
		"\t tr train_num speed = Set the speed of the specified train.",
		"\t wh = Display the last modified sensor.",
		"\t start = Starts the train set.",
		"\t stop = Stops the train set.",
		"++ Route Finding Commands ++",
		"\t path start dest = Display the path from start to dest.",
		"\t fstSw start dest = Display the first switch to change on path.",
		"\t fstRv start dest = Display first reverse on path."
		};

	// Quit
    if( sscanf( command, "q" )>= 0 ) {
		output( "Shell exiting.\r\n" );
		Destroy( WhoIs( "Idle" ) );
        Exit(  );
	// k1
    } else if( sscanf( command, "k1" )>=0 ) {	// Run Kernel 1
		Create( 0, &k1_firstUserTask );
		output( "K1 is done executing.\r\n" );
    // k2
	} else if( sscanf( command, "k2" )>=0 ) {	// Run Kernel 2
		Create( 0, &k2_firstUserTask );
		Destroy( WhoIs( "GameServer" ) );
		output( "K2 is done executing.\r\n" );
    // k3
	} else if( sscanf( command, "k3" )>=0 ) {	// Run Kernel 3
		Create( 0, &k3_firstUserTask );
		// K3 is asynchronous so we don't know when it will end
		//output( "K3 is done executing.\r\n" );
	// rv
	} else if( sscanf( command, "rv %d", &tsReq.train )>=0 ) {
    	tsReq.type = RV;
		trackCmd( &tsReq, tids );
	// st
    } else if( sscanf( command, "st %d", &tsReq.sw )>=0 ) {
		tsReq.type = ST;
		tsRpl = trackCmd( &tsReq, tids );
		output( "Switch %d is set to %c. \r\n", tsReq.sw, switch_dir( tsRpl.dir ) );
	// sw
    } else if( sscanf( command, "sw %d %c", &tsReq.sw, tmpStr1 )>=0 ) {
		tsReq.type = SW;
		tsReq.dir = switch_init( tmpStr1[0] );
		trackCmd( &tsReq, tids );
	// tr
	} else if( sscanf( command, "tr %d %d", &tsReq.train, &tsReq.speed )>= 0 ) {
		tsReq.type = TR;
		trackCmd( &tsReq, tids );
	// wh
    } else if( sscanf( command, "wh" )>=0 ) {
		tsReq.type = WH;
		tsRpl = trackCmd( &tsReq, tids );
		if( tsRpl.sensor < 0 ) {
			output( "No sensor triggered yet." );
		} else {
			output( "Last sensor triggered was %c%d", 
				sensor_bank( tsRpl.sensor ), sensor_num( tsRpl.sensor ) );
		}
		output( "( updated %d ms ago ).\r\n", tsRpl.ticks * MS_IN_TICK );
	// start
    } else if( sscanf( command, "start" )>=0 ) {
		tsReq.type = START;
		trackCmd( &tsReq, tids );
	// stop
    } else if( sscanf( command, "stop" )>=0 ) {
		tsReq.type = STOP;
		trackCmd( &tsReq, tids );
	// cache ON
	} else if( sscanf( command, "cache ON" )>= 0 ) {
		cache_on(  );
	// cache OFF
	} else if( sscanf( command, "cache OFF" )>= 0 ) {
		cache_off(  );
	// path
	} else if( sscanf( command, "path %s %s", tmpStr1, tmpStr2 )>= 0 ) {
		rpReq.type = CONVERT_SENSOR;
		strncpy( rpReq.name, (const char *) tmpStr1, 5 );
		rpRpl = rpCmd( &rpReq, tids->rp );

		tmpInt = rpRpl.idx;
		
		rpReq.type = CONVERT_IDX;
		strncpy( rpReq.name, (const char *) tmpStr2, 5 );
		rpRpl = rpCmd( &rpReq, tids->rp );

		if( ( tmpInt == NOT_FOUND )||( rpRpl.err == NOT_FOUND )||
			( tmpInt == INVALID_NODE_NAME )||( rpRpl.err == INVALID_NODE_NAME ) ) {
			output( "Invalid node name.\r\n" );
		} else {
			rpReq.type       = DISPLAYROUTE;
			rpReq.lastSensor = tmpInt;
			rpReq.destIdx    = rpRpl.idx;
			output( "Displaying from %d to %d\r\n", tmpInt, rpRpl.idx );
			rpCmd( &rpReq, tids->rp );
		}
	// first switch
	} else if( sscanf( command, "fstSw %s %s", tmpStr1, tmpStr2 )>= 0 ) {
		rpReq.type = CONVERT_IDX;
		strncpy( rpReq.name, (const char *) tmpStr1, 5 );
		rpRpl = rpCmd( &rpReq, tids->rp );

		tmpInt = rpRpl.idx;
		
		rpReq.type = CONVERT_IDX;
		strncpy( rpReq.name, (const char *) tmpStr2, 5 );
		rpRpl = rpCmd( &rpReq, tids->rp );
		
		if( ( tmpInt == NOT_FOUND )||( rpRpl.err == NOT_FOUND )||
			( tmpInt == INVALID_NODE_NAME )||( rpRpl.err == INVALID_NODE_NAME ) ) {
			output( "Invalid node name.\r\n" );
		} else {
			rpReq.type = DISPLAYFSTSW;
			rpReq.nodeIdx1 = tmpInt;
			rpReq.nodeIdx2 = rpRpl.idx;
			rpCmd( &rpReq, tids->rp );
		}
	// first reverse
	} else if( sscanf( command, "fstRv %s %s", tmpStr1, tmpStr2 )>= 0 ) {
		rpReq.type = CONVERT_IDX;
		strncpy( rpReq.name, (const char *) tmpStr1, 5 );
		rpRpl = rpCmd( &rpReq, tids->rp );

		tmpInt = rpRpl.idx;
		
		rpReq.type = CONVERT_IDX;
		strncpy( rpReq.name, (const char *) tmpStr2, 5 );
		rpRpl = rpCmd( &rpReq, tids->rp );
			
		if( ( tmpInt == NOT_FOUND )||( rpRpl.err == NOT_FOUND )||
			( tmpInt == INVALID_NODE_NAME )||( rpRpl.err == INVALID_NODE_NAME ) ) {
			output( "Invalid node name.\r\n" );
		} else {
			rpReq.type = DISPLAYFSTRV;
			rpReq.nodeIdx1 = tmpInt;
			rpReq.nodeIdx2 = rpRpl.idx;
			rpCmd( &rpReq, tids->rp );
		}
	// predict
	} else if( sscanf( command, "predict %s", tmpStr1 )>= 0 ) {
		rpReq.type = CONVERT_IDX;
		strncpy( rpReq.name, (const char *) tmpStr1, 5 );
		rpRpl = rpCmd( &rpReq, tids->rp );
		
		if( ( rpRpl.idx == NOT_FOUND )||( rpRpl.err == INVALID_NODE_NAME ) ) {
			output( "Invalid node name.\r\n" );
		} else {
			rpReq.type = DISPLAYPREDICT;
			rpReq.nodeIdx1 = rpRpl.idx;
			rpCmd( &rpReq, tids->rp );
		}
	// Reserve
	} else if( sscanf( command, "reserve %s", tmpStr1 )>= 0 ) {
		rpReq.type = CONVERT_IDX;
		strncpy( rpReq.name, (const char *) tmpStr1, 5 );
		rpRpl = rpCmd( &rpReq, tids->rp );
		
		if( ( rpRpl.idx == NOT_FOUND )||( rpRpl.err == INVALID_NODE_NAME ) ) {
			output( "Invalid node name.\r\n" );
		} else {
			printf ("SHELL IS RESERVING\r\n");
			rpReq.type 	   = RESERVE;
			rpReq.nodeIdx1 = rpRpl.idx;
			rpReq.trainId  = 12; // TODO:
			rpCmd( &rpReq, tids->rp );
		}

	} else if( sscanf( command, "wolf %s", tmpStr1 )>= 0 ) {
		rpReq.type = CONVERT_IDX;
		strncpy( rpReq.name, (const char*) tmpStr1, 5 );
		rpRpl = rpCmd( &rpReq, tids->rp );
	
		if( ( rpRpl.idx == NOT_FOUND )||( rpRpl.err == INVALID_NODE_NAME ) ) {
			output( "Invalid node name.\r\n" );
		} else {
			rpReq.type     = RESERVE;
			rpReq.nodeIdx1 = rpRpl.idx;
			rpReq.trainId  = 12; // TODO: Remove this after Demo 1
			rpCmd( &rpReq, tids->rp );
		}
	// go
	} else if( sscanf( command, "go %s", tmpStr1 )>= 0 ) {
		shell_cmdTrain( tids, tmpStr1, 0, DRIVE );
	// cal
	} else if( sscanf( command, "cal %s", tmpStr1 )>= 0 ) {
		shell_cmdTrain( tids, tmpStr1, 0, CAL_STOP );
    // Help
	} else if( sscanf( command, "h" )>=0 ) {
		char **cmd;
		foreach( cmd, commands ) {
			output( "%s\r\n", *cmd );
		}
	// Nothing was entered
	} else if( command[0] == '\0' ) {
	// Unknown command
	} else {
		output( "Unknown command: '%s'\r\n", command );
	}
}

