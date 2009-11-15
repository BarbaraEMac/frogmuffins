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

#define INPUT_LEN   60
#define INPUT_HIST  1
#define IO			tids.ios2

#define output(args...) cprintf(IO, "\033[36m"); cprintf(IO, args); cprintf(IO, "\033[37m")

// Private Stuff
// ----------------------------------------------------------------------------
typedef struct {
	TID ns;
	TID cs;
	TID ios1;
	TID ios2;
	TID ts;
	TID rp;
	TID tr1Tid;	// TODO: Make this numbering dynamic?
	TID idle;
} TIDs;

// Use this function to grab a line of data before the shell starts.
void shell_inputData (TIDs tids, char *input, int defaultLen );

void shell_initTrack (TIDs tids, char *input);
void shell_initTrain (TIDs tids, char *input);

/**
 * Given an input command, error check it and execute it.
 */
void shell_exec ( char *command, TIDs tids);
void shell_run ( TIDs tids );

RPShellReply rpCmd ( RPRequest *req, TID rpTid );

// ----------------------------------------------------------------------------

// A fun idle task that counts to high numbers
void idleTask () {
	debug ("idleTask running\r\n");
	int i = 0;
	RegisterAs("Idle");

	while ( 1 ) {
		//if ( (i % 20) == 0 ) {
		//	printf(COM2, "IDLE=%d\r\n", i);
		//}
		i = i + 1;
	}
}

void bootstrap ( ) {
	TIDs tids;

	// Create the name server
	// Create the Serial I/O server
	tids.ns   = Create (2, &ns_run);
	tids.ios1 = Create (2, &ios1_run);
    tids.ios2 = Create (2, &ios2_run);
	output ("Initializing the name and serial io servers. \r\n");
	
	// Create the clock server
	tids.cs = Create (2, &cs_run);
	output ("Initializing the clock server. \r\n");

	// Create the train controller
	tids.ts = Create (2, &ts_run);
	output ("Initializing the train controller. \r\n");
	
	// Create the routeplanner
	tids.rp = Create (7, &rp_run);
	output ("Initializing the route planner. \r\n");
	
	// Create the idle task
	tids.idle = Create (9, &idleTask);

	output("Running on board %08x. \r\n", board_id() );

	// Run the shell
	shell_run( tids );

}

void shell_run ( TIDs tids ) {
    debug ("shell_run\r\n");

	// Initialize variables
    char ch, *input, history[INPUT_HIST][INPUT_LEN];
    int i = 0, h = 0;
	int	time, tens, secs, mins;

	input = history[h++];

	// Initialize the track we want to use.
	shell_initTrack (tids, input);
	
	// Initialize the first train.
	shell_initTrain (tids, input);

	output ("Type 'h' for a list of commands.\r\n");
    
	time = Time(tids.cs)/2;
	tens = time % 10;
	secs = (time / 10) % 60;
	mins = time / 600;
	output ("\r%02d:%02d:%01d> ", mins, secs, tens);
	
	i=0;
	// Main loop
    FOREVER {

        input[i] = 0;						// Clear the next character
		ch = Getc( tids.ios2 );

		time = Time( tids.cs ) / (100 / MS_IN_TICK);
		tens = time % 10;
		secs = (time / 10) % 60;
		mins = time / 600;

		switch ( ch ) {
			case '\r': // Enter was pressed
				output( "\n\r" );
				shell_exec(input, tids);
				
				// Clear the input for next line
				/*input = history[h++];
				h %= INPUT_HIST;*/
				i = 0;
				output ("\r%02d:%02d:%01d> ", mins, secs, tens);
				break;
			case '\b': // Backspace was pressed
			case 127:
				if( i > 0 ) {
					output( "\b \b" );
					i --;
				}
				break;
			case '\033': // Escape sequence
				ch = Getc( tids.ios2 );	// read the '['
				if( ch != '[' ) break;
				ch = Getc( tids.ios2 );
				output( "\r\n command %c detected.", ch );
				switch( ch ) {
					case 'A': // Up was pressed
					
						break;
					default:

						break;
				}

				break;

			default:	// Update the position in the command string
				output( "%c", ch );
				if( ch < 32 || ch > 126 ) {
					output("%d", input[i]);
				}
				input[i++] = ch;
				break;
		}
        if( i == INPUT_LEN - 1 ) {	i-- ; output("\b");}
    }
}

void shell_inputData (TIDs tids, char *input, int defaultLen ) {
	char ch;
    int i = 0;
	
	// Print out the default value
	while ( i < defaultLen ) {
		output("%c", input[i++] );
	}

	FOREVER {
        input[i] = 0;	// Clear the next character
		ch = Getc( tids.ios2 );

		switch ( ch ) {
			case '\r': // Enter was pressed
				
				output( "\n\r" );
				return;
				break;
			case '\b': // Backspace was pressed
			case 127:
				if( i > 0 ) {
					output( "\b \b" );
					i --;
				}
				break;

			default:	// Update the position in the command string
				output( "%c", ch );
				if( ch < 32 || ch > 126 ) {
					output("%d", input[i]);
				}
				input[i++] = ch;
				break;
		}
        if( i == INPUT_LEN - 1 ) {	i-- ; output("\b");}
    }
}

void shell_initTrack (TIDs tids, char *input) {
	int err;
	
	if( board_id() == 0x9224e4a1 ) {
		input[0] = 'A';
	} else if ( board_id() == 0x9224c1a8 ) {
		input[0] = 'B';
	} else {
		input[0] = '\0';
	}

	output ("Track: ");
	shell_inputData(tids, input, 1 );
	// Tell the Route Planner which track we are using
	Send (tids.rp, (char*)&input[0], sizeof(char),
				   (char*)&err, 	 sizeof(int));

	if ( err < NO_ERROR ) {
		output ("Invalid Track ID. Using Track B.\r\n");
	}
}

void shell_initTrain (TIDs tids, char *input) {
	RPRequest 	 rpReq;
	RPShellReply rpRpl;
	TrainInit 	 trInit;

	rpReq.nodeIdx1 = 0;
	rpReq.nodeIdx2 = 0;
	
	output ("Train Id: ");
	shell_inputData(tids, input, 0);
	trInit.id = atoi((const char**)&input);

	output ("Current Sensor: " );
	shell_inputData(tids, input, 0);
	
	rpReq.type = CONVERT_SENSOR;
	strncpy(rpReq.name, (const char*)input, 5);
	rpRpl = rpCmd ( &rpReq, tids.rp );

	trInit.currLoc = rpRpl.idx;

	output ("Destination: ");
	shell_inputData(tids, input, 0);
	
	rpReq.type = CONVERT_IDX;
	strncpy(rpReq.name, (const char*)input, 5);
	rpRpl = rpCmd ( &rpReq, tids.rp );
	
	trInit.dest = rpRpl.idx;

	// Create the first train!
	tids.tr1Tid = Create ( 5, &train_run );
	output ("Creating the first train! (%d)\r\n", tids.tr1Tid);

	// Tell the train its init info.
	Send ( tids.tr1Tid, (char*)&trInit, sizeof(TrainInit),
						(char*)&trInit.id, sizeof(int) );
}

// Execute a train command
TSReply trainCmd ( TSRequest *req, TID tsTid ) {
	TSReply	rpl;
	
	Send( tsTid, (char*) req, sizeof(TSRequest), (char*) &rpl, sizeof(rpl) ); 
	return rpl;
}

// Execute a Route Planner Command
RPShellReply rpCmd ( RPRequest *req, TID rpTid ) {
	RPShellReply	rpl;
	
	Send( rpTid, (char*) req,  sizeof(RPRequest),
				 (char*) &rpl, sizeof(rpl) ); 
	return rpl;
}

// Execute the command passed in
void shell_exec( char *command, TIDs tids ) {
	int 		 i;
	char 		 tmpStr1[12];
	char 		 tmpStr2[12];
	int			 tmpInt;

	TSRequest	 tsReq;
	TSReply		 tsRpl;

	RPRequest	 rpReq;
	RPShellReply rpRpl;

	// Clear the request
	rpReq.name[0]  = 0;
	rpReq.nodeIdx1 = 0;
	rpReq.nodeIdx2 = 0;

	char *commands[] = {
		"\th = Help!", 
		"\tk1 = Execute kernel 1 user tasks.", 
		"\tk2 = Execute kernel 2 user tasks.", 
		"\tk3 = Execute kernel 3 user tasks.", 
	//	"cache ON/OFF = turn the instruction cache on/off respectively",
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
    if ( sscanf(command, "q") >= 0 ) {
		output( "Shell exiting.\r\n" );
		Destroy( WhoIs("Idle") );
        Exit();
	// k1
    } else if( sscanf(command, "k1") >=0 ) {	// Run Kernel 1
		Create (0, &k1_firstUserTask);
		output( "K1 is done executing.\r\n" );
    // k2
	} else if( sscanf(command, "k2") >=0 ) {	// Run Kernel 2
		Create (0, &k2_firstUserTask);
		Destroy ( WhoIs("GameServer") );
		output( "K2 is done executing.\r\n" );
    // k3
	} else if( sscanf(command, "k3") >=0 ) {	// Run Kernel 3
		Create (0, &k3_firstUserTask);
		// K3 is asynchronous so we don't know when it will end
		//output( "K3 is done executing.\r\n" );
	// rv
	} else if( sscanf(command, "rv %d", &tsReq.train) >=0) {
    	tsReq.type = RV;
		trainCmd( &tsReq, tids.ts );
	// st
    } else if( sscanf(command, "st %d", &tsReq.sw) >=0 ) {
		tsReq.type = ST;
		tsRpl = trainCmd( &tsReq, tids.ts );
		output( "Switch %d is set to %c. \r\n", tsReq.sw, switch_dir( tsRpl.dir ) );
	// sw
    } else if( sscanf(command, "sw %d %c", &tsReq.sw, tmpStr1) >=0 ) {
		tsReq.type = SW;
		tsReq.dir = switch_init( tmpStr1[0] );
		trainCmd( &tsReq, tids.ts );
	// tr
	} else if( sscanf(command, "tr %d %d", &tsReq.train, &tsReq.speed) >= 0 ) {
		tsReq.type = TR;
		trainCmd( &tsReq, tids.ts );
	// wh
    } else if( sscanf(command, "wh") >=0 ) {
		tsReq.type = WH;
		tsRpl = trainCmd( &tsReq, tids.ts );
		if( tsRpl.sensor < 0 ) {
			output( "No sensor triggered yet." );
		} else {
			output( "Last sensor triggered was %c%d", 
				sensor_bank( tsRpl.sensor ), sensor_num( tsRpl.sensor ));
		}
		output (" (updated %d ms ago).\r\n", tsRpl.ticks * MS_IN_TICK );
	// start
    } else if( sscanf(command, "start") >=0 ) {
		tsReq.type = START;
		trainCmd( &tsReq, tids.ts );
	// stop
    } else if( sscanf(command, "stop") >=0 ) {
		tsReq.type = STOP;
		trainCmd( &tsReq, tids.ts );
	// cache ON
	} else if( sscanf(command, "cache ON") >= 0 ) {
		cache_on();
	// cache OFF
	} else if( sscanf(command, "cache OFF") >= 0 ) {
		cache_off();
	// path
	} else if( sscanf(command, "path %s %s", tmpStr1, tmpStr2) >= 0 ) {
		rpReq.type = CONVERT_IDX;
		strncpy(rpReq.name, (const char*)tmpStr1, 5);
		rpRpl = rpCmd ( &rpReq, tids.rp );

		tmpInt = rpRpl.idx;
		
		rpReq.type = CONVERT_IDX;
		strncpy(rpReq.name, (const char*)tmpStr2, 5);
		rpRpl = rpCmd ( &rpReq, tids.rp );

		if ( (tmpInt == NOT_FOUND)         || (rpRpl.err == NOT_FOUND) ||
			 (tmpInt == INVALID_NODE_NAME) || (rpRpl.err == INVALID_NODE_NAME) ) {
			output ("Invalid node name.\r\n");
		} else {
			rpReq.type = DISPLAYROUTE;
			rpReq.nodeIdx1 = tmpInt;
			rpReq.nodeIdx2 = rpRpl.idx;
			rpCmd ( &rpReq, tids.rp );
		}
	// first switch
	} else if( sscanf(command, "fstSw %s %s", tmpStr1, tmpStr2) >= 0 ) {
		rpReq.type = CONVERT_IDX;
		strncpy(rpReq.name, (const char*)tmpStr1, 5);
		rpRpl = rpCmd ( &rpReq, tids.rp );

		tmpInt = rpRpl.idx;
		
		rpReq.type = CONVERT_IDX;
		strncpy(rpReq.name, (const char*)tmpStr2, 5);
		rpRpl = rpCmd ( &rpReq, tids.rp );
		
		if ( (tmpInt == NOT_FOUND)         || (rpRpl.err == NOT_FOUND) ||
			 (tmpInt == INVALID_NODE_NAME) || (rpRpl.err == INVALID_NODE_NAME) ) {
			output ("Invalid node name.\r\n");
		} else {
			rpReq.type = DISPLAYFSTSW;
			rpReq.nodeIdx1 = tmpInt;
			rpReq.nodeIdx2 = rpRpl.idx;
			rpCmd ( &rpReq, tids.rp );
		}
	// first reverse
	} else if( sscanf(command, "fstRv %s %s", tmpStr1, tmpStr2) >= 0 ) {
		rpReq.type = CONVERT_IDX;
		strncpy(rpReq.name, (const char*)tmpStr1, 5);
		rpRpl = rpCmd ( &rpReq, tids.rp );

		tmpInt = rpRpl.idx;
		
		rpReq.type = CONVERT_IDX;
		strncpy(rpReq.name, (const char*)tmpStr2, 5);
		rpRpl = rpCmd ( &rpReq, tids.rp );
			
		if ( (tmpInt == NOT_FOUND)         || (rpRpl.err == NOT_FOUND) ||
			 (tmpInt == INVALID_NODE_NAME) || (rpRpl.err == INVALID_NODE_NAME) ) {
			output ("Invalid node name.\r\n");
		} else {
			rpReq.type = DISPLAYFSTRV;
			rpReq.nodeIdx1 = tmpInt;
			rpReq.nodeIdx2 = rpRpl.idx;
			rpCmd ( &rpReq, tids.rp );
		}
	// predict
	} else if( sscanf(command, "predict %s", tmpStr1) >= 0 ) {
		rpReq.type = CONVERT_IDX;
		strncpy(rpReq.name, (const char*)tmpStr1, 5);
		rpRpl = rpCmd ( &rpReq, tids.rp );
		
		if ( (rpRpl.idx == NOT_FOUND) || (rpRpl.err == INVALID_NODE_NAME) ) {
			output ("Invalid node name.\r\n");
		} else {
			rpReq.type = DISPLAYPREDICT;
			rpReq.nodeIdx1 = rpRpl.idx;
			rpCmd ( &rpReq, tids.rp );
		}
    // Help
	} else if( sscanf(command, "h") >=0 ) {
		for( i = 0; i < (sizeof( commands ) / sizeof( char * )); i++ ) {
			output( "%s\r\n", commands[i] );
		}
	// Nothing was entered
	} else if( command[0] == '\0' ) {
	// Unknown command
	} else {
		output("Unknown command: '%s'\r\n", command );
	}
}

