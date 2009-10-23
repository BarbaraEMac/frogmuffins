 /*
 * dgoc_a1.c
 */
#define DEBUG

#include <bwio.h>
#include <string.h>
#include <ts7200.h>

#include "clockserver.h"
#include "debug.h"
#include "nameserver.h"
#include "requests.h"
#include "serialio.h"
#include "shell.h"
#include "task.h"
#include "traincontroller.h"

#define FOREVER     for( ; ; )
#define INPUT_LEN   60
#define INPUT_HIST  1

#define output(args...) bwputstr(COM2, "\033[36m"); bwprintf(COM2, args); bwputstr(COM2,"\033[37m")

void shell_run ( ) {
    debug ("shell_run\r\n");

	// Initialize variables
    char *input, history[INPUT_HIST][INPUT_LEN];
    int i = 0, h = 0, time;
	TID nsTid;
	TID csTid;
	TID iosTid;
	TID tcTid;
	CSRequest csReq;

	csReq.type  = TIME;
	csReq.ticks = 0;

    // Set up the communication ports
	bwsetfifo( COM2, OFF );
    
	// Create the name server
	debug ("Creating the name server. \r\n");
	nsTid = Create (2, &ns_run);
	
	// Create the clock server
	debug ("Creating the clock server. \r\n");
	csTid = Create (2, &cs_run);

	// Create the Serial I/O server
	debug ("Creating the serial io server. \r\n");
	iosTid = Create (2, &ios_run);

	// Create the train controller
	debug ("Creating the train controller. \r\n");
	tcTid = Create (2, &tc_run);
	
	output ("Type 'h' for a list of commands.\r\n");

	// Main loop
	int k =0, l = 0;
    input = history[h++];
	
	time = Time(csTid);
	output ("\r%02d:%02d:%02d> ", (((time*50)/1000)/60), (((time*50)/1000)%60),
		    ((time*50)%1000)); 
    FOREVER {

        input[i] = 0;						// Clear the next character
		k++; l++;
		if( k!=l ) { bwputstr(COM2, "\r\n GOTCHA \r\n"); }
		if( bwreadc( COM2, &(input[i]), 0 ) == 1 ) {
            if( input[i] == '\r' ) {        // Enter was pressed
				bwputstr ( COM2, "\n\r");
                input[i+1] = 0;
                
				shell_exec( input, tcTid );	// This may call Exit();
                
				// Clear the input for next line
                //input = history[h++];
                //h++; h &= INPUT_HIST;
                i = 0;
				time = Time(csTid);
				output ("\r%02d:%02d:%02d> ", (((time*50)/1000)/60),
						(((time*50)/1000)%60), ((time*50)%1000));
            } else if( input[i] == '\b'
                    || input[i] == 127) {	// Backspace was pressed
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
        if( i == INPUT_LEN ) {
            output( "\r\nToo many characters.\r\n" );
            i = 0;
			
			time = Time(csTid);
			output ("\r%02d:%02d:%02d> ", (((time*50)/1000)/60),
					(((time*50)/1000)%60), ((time*50)%1000));
		}
    }
}

// Execute the command passed in
void shell_exec( char *command, TID tcTid ) {
	TCRequest tcReq;
	int	  rpl;

    if ( sscanf(command, "q\r") >= 0 ) {
        Exit();
    } 
	else if( sscanf(command, "k1\r") >=0 ) {	// Run Kernel 1
		Create (0, &k1_firstUserTask);
		output( "K1 is done executing.\r\n" );
    }
	else if( sscanf(command, "k2\r") >=0 ) {	// Run Kernel 2
		Create (0, &k2_firstUserTask);
		output( "K2 is done executing.\r\n" );
    }
	else if( sscanf(command, "k3\r") >=0 ) {	// Run Kernel 3
		Create (0, &k3_firstUserTask);
		
		// K3 is asynchronous so we don't know when it will end
		//output( "K3 is done executing.\r\n" );
	}
	else if( sscanf(command, "rv %d\r", &tcReq.arg1) >=0 ) {
    	tcReq.type = RV;
		
		Send(tcTid, (char*) &tcReq, sizeof(TCRequest), (char*)&rpl, sizeof(int));
    } 
	else if( sscanf(command, "st %d\r", &tcReq.arg1) >=0 ) {
		tcReq.type = ST;
		
		Send(tcTid, (char*) &tcReq, sizeof(TCRequest), (char*)&rpl, sizeof(int));
    }
	else if( sscanf(command, "sw %d %c\r", &tcReq.arg1, (char*)&tcReq.arg2) >=0 ) {
		tcReq.type = SW;
		
		Send(tcTid, (char*) &tcReq, sizeof(TCRequest), (char*)&rpl, sizeof(int));
	}
	else if( sscanf(command, "tr %d %d\r", &tcReq.arg1, &tcReq.arg2) >= 0 ) {
    	tcReq.type = TR;

		Send(tcTid, (char*) &tcReq, sizeof(TCRequest), (char*)&rpl, sizeof(int));
    } 
	else if( sscanf(command, "wh\r") >=0 ) {
		tcReq.type = WH;
		
		Send(tcTid, (char*) &tcReq, sizeof(TCRequest), (char*)&rpl, sizeof(int));
    }
	else if( sscanf(command, "h\r") >=0 ) {	// Get help!
		output( "\t%s\r\n\t%s\r\n\t%s\r\n\t%s\r\n\t%s\r\n\t%s\r\n\t%s\r\n\t%s\r\n\t%s\r\n\t%s\r\n", 
				"h = Help!", 
				"k1 = Execute kernel 1 user tasks.", 
				"k2 = Execute kernel 2 user tasks.", 
				"k3 = Execute kernel 3 user tasks.", 
				"++ Train Controller Commands ++",
				"\t rv train_num  = Reverse specified train.",
				"\t st switch_num = Display status of switch.",
				"\t sw switch_num dir = Switch the switch in the sepcified direction.",
				"\t tr train_num speed = Set the speed of the specified train.",
				"\t wh = Display the last modified sensor.");
	}
	else {
		output("\r\nUnknown command: %s\r\n", command);
	}
}

