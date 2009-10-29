 /*
 * dgoc_a1.c
 */
#define DEBUG 2

#include <bwio.h>
#include <string.h>
#include <ts7200.h>

#include "debug.h"
#include "requests.h"
#include "servers.h"
#include "shell.h"
#include "task.h"
#include "traincontroller.h"

#define FOREVER     for( ; ; )
#define INPUT_LEN   60
#define INPUT_HIST  1

#define output(args...) bwputstr(COM2, "\033[36m"); bwprintf(COM2, args); bwputstr(COM2,"\033[37m")

void shell_run ( ) {
    debug ("shell_run\r\n");
	char ch;

	// Initialize variables
    char *input, history[INPUT_HIST][INPUT_LEN];
    int i = 0, h = 0;
	TID nsTid;
	TID csTid;
	TID ios1Tid, ios2Tid;
	TID tcTid;
	CSRequest csReq;

	csReq.type  = TIME;
	csReq.ticks = 0;

	// Create the name server
	debug ("Creating the name server. \r\n");
	nsTid = Create (2, &ns_run);
	
	// Create the clock server
	debug ("Creating the clock server. \r\n");
	csTid = Create (2, &cs_run);

	// Create the Serial I/O server
	debug ("Creating the serial io server. \r\n");
	ios1Tid = Create (2, &ios1_run);
//	ios2Tid = Create (2, &ios2_run);

	// Create the train controller
	debug ("Creating the train controller. \r\n");
	tcTid = Create (2, &tc_run);
	
	output ("Type 'h' for a list of commands.\r\n");

    input = history[h++];
	int	time, tens, secs, mins;
 
	time = Time(csTid)/2;
	tens = time % 10;
	secs = (time / 10) % 60;
	mins = time / 600;
	output ("\r%02d:%02d:%02d> ", mins, secs, tens);

	// Main loop
    FOREVER {

        input[i] = 0;						// Clear the next character

		time = Time(csTid)/2;
		tens = time % 10;
		secs = (time / 10) % 60;
		mins = time / 600;

		if( bwreadc( COM2, &(input[i]), 0 ) == 1 ) {
            if( input[i] == '\r' ) {        // Enter was pressed
				bwputstr ( COM2, "\n\r");
                input[i+1] = 0;
                
				shell_exec( input, tcTid, ios1Tid, ios2Tid);// This may call Exit();
                
				// Clear the input for next line
                //input = history[h++];
                //h++; h &= INPUT_HIST;
                i = 0;
				output ("\r%02d:%02d:%02d> ", mins, secs, tens);
            } else if( input[i] == '\b' || input[i] == 127) {	// Backspace was pressed
				if( i > 0 ) {
					bwputstr ( COM2, "\b \b");
    	            
					input[i] = 0;
            	    i --;
				}
            } else {                        // Update the position in the command string
				output ("%c", input[i]);
				if( input[i] < 32 || input[i] > 126 ) {
					output("%d", input[i]);
				}
				i ++;
            }
        }
        if( i == INPUT_LEN - 1 ) {	i-- ; }
    }
}


// Execute a train command
int trainCmd ( TCRequest *tcReq, int tcTid ) {
	int	  rpl;
	Send(tcTid, (char*) &tcReq, sizeof(TCRequest), (char*)&rpl, sizeof(int)); 
	return rpl;
}

// Execute the command passed in
void shell_exec( char *command, TID tcTid, TID ios1Tid, TID ios2Tid ) {
	TCRequest tcReq;
	char *commands[] = {
		"h = Help!", 
		"k1 = Execute kernel 1 user tasks.", 
		"k2 = Execute kernel 2 user tasks.", 
		"k3 = Execute kernel 3 user tasks.", 
		"cache ON/OFF = turn the instruction cache on/off respectively",
		"++ Train Controller Commands ++",
		"\t rv train_num  = Reverse specified train.",
		"\t st switch_num = Display status of switch.",
		"\t sw switch_num dir = Switch the switch in the sepcified direction.",
		"\t tr train_num speed = Set the speed of the specified train.",
		"\t wh = Display the last modified sensor." };
	int i;

	// Quit
    if ( sscanf(command, "q\r") >= 0 ) {
        Exit();
	// k1
    } else if( sscanf(command, "k1\r") >=0 ) {	// Run Kernel 1
		Create (0, &k1_firstUserTask);
		output( "K1 is done executing.\r\n" );
    // k2
	} else if( sscanf(command, "k2\r") >=0 ) {	// Run Kernel 2
		Create (0, &k2_firstUserTask);
		Destroy (WhoIs("GameServer"));
		output( "K2 is done executing.\r\n" );
    // k3
	} else if( sscanf(command, "k3\r") >=0 ) {	// Run Kernel 3
		Create (0, &k3_firstUserTask);
		// K3 is asynchronous so we don't know when it will end
		//output( "K3 is done executing.\r\n" );
	// rv
	} else if( sscanf(command, "rv %d\r", &tcReq.arg1) >=0 ) {
    	tcReq.type = RV;
		trainCmd( &tcReq, tcTid );
	// st
    } else if( sscanf(command, "st %d\r", &tcReq.arg1) >=0 ) {
		tcReq.type = ST;
		trainCmd( &tcReq, tcTid );
	// sw
    } else if( sscanf(command, "sw %d %c\r", &tcReq.arg1, (char*)&tcReq.arg2) >=0 ) {
		tcReq.type = SW;
		trainCmd( &tcReq, tcTid );
	// tr
	} else if( sscanf(command, "tr %d %d\r", &tcReq.arg1, &tcReq.arg2) >= 0 ) {
    	tcReq.type = TR;
		trainCmd( &tcReq, tcTid );
	// wh
    } else if( sscanf(command, "wh\r") >=0 ) {
		tcReq.type = WH;
		trainCmd( &tcReq, tcTid );
	// cache ON
	} else if( sscanf(command, "cache ON\r") >= 0 ) {
		cache_on();
	// cache OFF
	} else if( sscanf(command, "cache OFF\r") >= 0 ) {
		cache_off();
    // Help
	} else if( sscanf(command, "h\r") >=0 ) {	// Get help!
		for( i = 0; i < (sizeof( commands ) / sizeof( char * )); i++ ) {
			output( "\t%s\r\n", commands[i] );
		}
	// Nothing was entered
	} else if( command[0] == '\r' ) {
	// Unkknown command
	} else {
		output("Unknown command: %s\r\n", command);
	}
}

