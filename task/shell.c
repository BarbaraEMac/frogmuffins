 /*
 * dgoc_a1.c
 */
//#define DEBUG

#include <bwio.h>
#include <string.h>
#include <ts7200.h>

#include "clockserver.h"
#include "debug.h"
#include "nameserver.h"
#include "requests.h"
#include "shell.h"
#include "task.h"

#define FOREVER     for( ; ; )
#define INPUT_LEN   256
#define INPUT_HIST  1

#define output(args...) bwputstr(COM2, "\033[36m"); bwprintf(COM2, args); bwputstr(COM2,"\033[37m")

void shell_run ( ) {
    debug ("shell_run\r\n");

	// Initialize variables
    char *input, history[INPUT_HIST][INPUT_LEN];
    int i = 0, h = 0;

    // Set up the communication ports
	bwsetfifo( COM2, OFF );
    
	// Create the name server
	debug ("Creating the name server. \r\n");
	Create (1, &ns_run);
	
	// Create the clock server
	debug ("Creating the clock server. \r\n");
	Create (2, &cs_run);
	
	output ("Type 'h' for a list of commands.\r\n");

	// Main loop
    input = history[h++];
	output ("\r > "); 
    FOREVER {

        input[i] = 0;						// Clear the next character

		if( bwreadc( COM2, &(input[i]), 0 ) ) {
            if( input[i] == '\r' ) {        // Enter was pressed
				output ("\n\r");
                input[i+1] = 0;
                
				shell_exec( input );		// This may call Exit();
                
				// Clear the input for next line
                //input = history[h++];
                //h++; h &= INPUT_HIST;
                i = 0;
				output ("\r > "); 
            } else if( input[i] == '\b'
                    || input[i] == 127) {	// Backspace was pressed
                output ("%s", "\b \b");
                
				input[i] = 0;
                i --; if( i < 0 ) i = 0;
            } else {                        // Update the position in the command string
                output ("%c", input[i]);
				i ++;
            }
            //bwprintf( COM2, "\n\rCODE(%d)\n\r", input[i-1] );
        }
        if( i == INPUT_LEN ) {
            output( "Too many characters.\r\n" );
            i = 0;
			
			output ("\r > "); 
        }
    }
}

// Execute the command passed in
void shell_exec( char *command ) {
    int mask;

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
	else if( sscanf(command, "ua %x\r", &mask) >=0 ) {
        output( "Changing UART1 to %04x.\n\r", mask );
        int *flag = (int *)( UART1_BASE + UART_LCRH_OFFSET );
        *flag = mask;
        output( "UART1 SETTINGS: 0x%04x\n\r", *flag );
    }
	else if( sscanf(command, "h\r") >=0 ) {	// Get help!
		output( "\t%s\r\n\t%s\r\n\t%s\r\n\t%s\r\n\t%s\r\n", 
				"h = Help!", 
				"k1 = Execute kernel 1 user tasks.", 
				"k2 = Execute kernel 2 user tasks.", 
				"k3 = Execute kernel 3 user tasks.", 
				"ua = Change the UART." );
	}
}

