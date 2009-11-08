 /*
 * Shell for Kernel 
 * becmacdo
 * dgoc
 */
#define DEBUG 1 

#include <string.h>
#include <ts7200.h>

#include "debug.h"
#include "requests.h"
#include "servers.h"
#include "shell.h"
#include "task.h"
#include "train.h"

#define FOREVER     for( ; ; )
#define INPUT_LEN   60
#define INPUT_HIST  1
#define IO			ios2Tid

#define output(args...) cprintf(IO, "\033[36m"); cprintf(IO, args); cprintf(IO, "\033[37m")

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
	TID nsTid;
	TID csTid;
	TID ios1Tid, ios2Tid;
	TID tcTid;
	TID idle;

	// Create the name server
//	output ("Creating the name server. \r\n");
	nsTid = Create (2, &ns_run);
	
	// Create the Serial I/O server
	ios1Tid = Create (2, &ios1_run);
	ios2Tid = Create (2, &ios2_run);
	output ("Initializing the serial io servers. \r\n");
	
	// Create the clock server
	csTid = Create (2, &cs_run);
	output ("Initializing the clock server. \r\n");

	
	// Create the train controller
	tcTid = Create (2, &tc_run);
	output ("Initializing the train controller. \r\n");
	
	// Create the idle task
	idle = Create (9, &idleTask);

	output ("Type 'h' for a list of commands.\r\n");


    input = history[h++];
	int	time, tens, secs, mins;
 
	time = Time(csTid)/2;
	tens = time % 10;
	secs = (time / 10) % 60;
	mins = time / 600;
	output ("\r%02d:%02d:%01d> ", mins, secs, tens);
	
	i=0;
	// Main loop
    FOREVER {

        input[i] = 0;						// Clear the next character
		ch = Getc( ios2Tid );

		time = Time( csTid ) / (100 / MS_IN_TICK);
		tens = time % 10;
		secs = (time / 10) % 60;
		mins = time / 600;

		switch ( ch ) {
			case '\r': // Enter was pressed
				output( "\n\r" );
				shell_exec(input, tcTid, ios1Tid, ios2Tid);
				
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
				ch = Getc( ios2Tid );	// read the '['
				if( ch != '[' ) break;
				ch = Getc( ios2Tid );
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


// Execute a train command
TCReply trainCmd ( TCRequest *req, int tcTid ) {
	TCReply	rpl;
	
	Send( tcTid, (char*) req, sizeof(TCRequest), (char*) &rpl, sizeof(rpl) ); 
	return rpl;
}

// Execute the command passed in
void shell_exec( char *command, TID tcTid, TID ios1Tid, TID ios2Tid ) {
	TCRequest	tcReq;
	TCReply		tcRpl;
	char *commands[] = {
		"h = Help!", 
		"k1 = Execute kernel 1 user tasks.", 
		"k2 = Execute kernel 2 user tasks.", 
		"k3 = Execute kernel 3 user tasks.", 
	//	"cache ON/OFF = turn the instruction cache on/off respectively",
		"++ Train Controller Commands ++",
		"\t rv train_num  = Reverse specified train.",
		"\t st switch_num = Display status of switch.",
		"\t sw switch_num dir = Switch the switch in the sepcified direction.",
		"\t tr train_num speed = Set the speed of the specified train.",
		"\t wh = Display the last modified sensor.",
		"\t start = Starts the train set.",
		"\t stop = Stops the train set."};
	int i;

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
	} else if( sscanf(command, "rv %d", &tcReq.train) >=0) {
    	tcReq.type = RV;
		trainCmd( &tcReq, tcTid );
	// st
    } else if( sscanf(command, "st %d", &tcReq.sw) >=0 ) {
		tcReq.type = ST;
		tcRpl = trainCmd( &tcReq, tcTid );
		output( "Switch %d is set to %c. \r\n", tcReq.sw, tcRpl.dir );
	// sw
    } else if( sscanf(command, "sw %d %c", &tcReq.sw, &tcReq.dir) >=0 ) {
		tcReq.type = SW;
		trainCmd( &tcReq, tcTid );
	// tr
	} else if( sscanf(command, "tr %d %d", &tcReq.train, &tcReq.speed) >= 0 ) {
		tcReq.type = TR;
		trainCmd( &tcReq, tcTid );
	// wh
    } else if( sscanf(command, "wh") >=0 ) {
		tcReq.type = WH;
		tcRpl = trainCmd( &tcReq, tcTid );
		if( tcRpl.sensor == 0 ) {
			output( "No sensor triggered yet.\r\n" );
		} else {
			output( "Last sensor triggered was %c%d (updated %d ms ago).\r\n", 
					tcRpl.channel, tcRpl.sensor, tcRpl.ticks * MS_IN_TICK );
		}
	// start
    } else if( sscanf(command, "start") >=0 ) {
		tcReq.type = START;
		trainCmd( &tcReq, tcTid );
	// stop
    } else if( sscanf(command, "stop") >=0 ) {
		tcReq.type = STOP;
		trainCmd( &tcReq, tcTid );
	// cache ON
	} else if( sscanf(command, "cache ON") >= 0 ) {
		cache_on();
	// cache OFF
	} else if( sscanf(command, "cache OFF") >= 0 ) {
		cache_off();
    // Help
	} else if( sscanf(command, "h") >=0 ) {
		for( i = 0; i < (sizeof( commands ) / sizeof( char * )); i++ ) {
			output( "\t%s\r\n", commands[i] );
		}
		assert( 1 == 0 );
	// Nothing was entered
	} else if( command[0] == '\0' ) {
	// Unkknown command
	} else {
		output("Unknown command: '%s'\r\n", command );
	}
}

