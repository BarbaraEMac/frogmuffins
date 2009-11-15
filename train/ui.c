/*
 * User Interface for the train projet
 * becmacdo
 * dgoc
 */
#define DEBUG 2

#include <string.h>
#include <ts7200.h>

#include "debug.h"
#include "globals.h"
#include "model.h"
#include "requests.h"
#include "servers.h"
#include "ui.h"


// Private Stuff
// ----------------------------------------------------------------------------

void ui_clearScreen (int ios2Tid);

// ----------------------------------------------------------------------------

void ui_displayTrack () {
	int i;
	char ch;
	int  shellTid;
	int  track;
	int	 err = NO_ERROR;
	TrackModel model;

	// Get the model from the shell
	Receive(&shellTid, (char*)&ch, sizeof(ch));
	if ( ch != 'A' && ch != 'a' && ch != 'B' && ch != 'b' ) {
		err = INVALID_TRACK;
	}
	Reply  (shellTid,  (char*)&err, sizeof(int));

	track = ( ch == 'A' || ch == 'a' ) ? TRACK_A : TRACK_B;
	parse_model( track, &model );

	ui_clearScreen(WhoIs(SERIALIO2_NAME));


	for ( i = 0; i < model.num_nodes; i ++ ) {
		ui_printAt (WhoIs(SERIALIO2_NAME), 
					model.nodes[i].x, 
					model.nodes[i].y, 
					'+',
					YELLOW_FC, BLACK_BC);
	}

	Exit();

}

void ui_clearScreen (int ios2Tid) {
	cprintf(ios2Tid, "\033c");
}
void ui_printAt (int ios2Tid, int x, int y, char ch, 
				 ForeColour fc, BackColour bc ) {

	// Set the colour
	cprintf (ios2Tid, "\033[%dm", fc);
	cprintf (ios2Tid, "\033[%dm", bc);

	// Move the cursor
	cprintf (ios2Tid, "\033[%d;%dH", x, y);

	// Write the character
	cprintf (ios2Tid, "%c", ch);

	// Reset the colour
	cprintf (ios2Tid, "\033[%dm", DEFAULT_FC);
	cprintf (ios2Tid, "\033[%dm", DEFAULT_BC);
}

