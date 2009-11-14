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
void shell_inputData (TIDs tids, char *input);

/**
 * Given an input command, error check it and execute it.
 */
void shell_exec ( char *command, TIDs tids);

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

void shell_run ( ) {
    debug ("shell_run\r\n");

	// Initialize variables
    char ch, *input, history[INPUT_HIST][INPUT_LEN];
    int i = 0, h = 0;
	int	time, tens, secs, mins;
	TIDs tids;
	TrainInit trInit;

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

	input = history[h++];
/*
	output ("Track: ");
	shell_inputData(tids, input);
*/	
	output ("Train Id: ");
	shell_inputData(tids, input);
	trInit.id = atoi((const char**)&input);

	output ("Ahead Landmark Idx: " );
	shell_inputData(tids, input);
	trInit.currLoc = atoi((const char**)&input);

	output ("Behind Landmark Idx: " );
	shell_inputData(tids, input);
	trInit.prevLoc = atoi((const char**)&input);

	output ("Destination Landmark Idx: ");
	shell_inputData(tids, input);
	trInit.dest = atoi((const char**)&input);

	// Create the first train!
	tids.tr1Tid = Create ( 5, &train_run );
	output ("Creating the first train! (%d)\r\n", tids.tr1Tid);

	// Tell the train its init info.
	Send ( tids.tr1Tid, (char*)&trInit, sizeof(TrainInit),
						(char*)&trInit.id, sizeof(int) );


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

void shell_inputData (TIDs tids, char *input) {
	char ch;
    int i = 0;

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

// Execute a train command
TSReply trainCmd ( TSRequest *req, TID tsTid ) {
	TSReply	rpl;
	
	Send( tsTid, (char*) req, sizeof(TSRequest), (char*) &rpl, sizeof(rpl) ); 
	return rpl;
}

// Execute a Route Planner Command
RPReply rpCmd ( RPRequest *req, TID rpTid ) {
	RPReply	rpl;
	
	Send( rpTid, (char*)req, sizeof(RPRequest), (char*) &rpl, sizeof(rpl) ); 
	return rpl;
}

// Execute the command passed in
void shell_exec( char *command, TIDs tids ) {
	int i;
	TSRequest	tsReq;
	TSReply		tsRpl;

	RPRequest	rpReq;
	RPReply 	rpRpl;

	char *commands[] = {
		"h = Help!", 
		"k1 = Execute kernel 1 user tasks.", 
		"k2 = Execute kernel 2 user tasks.", 
		"k3 = Execute kernel 3 user tasks.", 
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
    } else if( sscanf(command, "sw %d %c", &tsReq.sw, &tsReq.dir) >=0 ) {
		tsReq.type = SW;
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
	} else if( sscanf(command, "path %d %d", &rpReq.idx1, &rpReq.idx2) >= 0 ) {
		rpReq.type = DISPLAYROUTE;
		rpCmd ( &rpReq, tids.rp );
	// first switch
	} else if( sscanf(command, "fstSw %d %d", &rpReq.idx1, &rpReq.idx2) >= 0 ) {
		rpReq.type = DISPLAYFSTSW;
		rpCmd ( &rpReq, tids.rp );
	// first reverse
	} else if( sscanf(command, "fstRv %d %d", &rpReq.idx1, &rpReq.idx2) >= 0 ) {
		rpReq.type = DISPLAYFSTRV;
		rpCmd ( &rpReq, tids.rp );

	} else if( sscanf(command, "predict %d", &rpReq.idx1) >= 0 ) {
		rpReq.type = DISPLAYPREDICT;

		rpCmd ( &rpReq, tids.rp );
    // Help
	} else if( sscanf(command, "h") >=0 ) {
		for( i = 0; i < (sizeof( commands ) / sizeof( char * )); i++ ) {
			output( "\t%s\r\n", commands[i] );
		}
	// Nothing was entered
	} else if( command[0] == '\0' ) {
	// Unknown command
	} else {
		output("Unknown command: '%s'\r\n", command );
	}
}

