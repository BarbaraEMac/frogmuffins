/*
 * User Interface for the train projet
 * becmacdo
 * dgoc
 */
#define DEBUG 1

#include <string.h>
#include <ts7200.h>

#include "buffer.h"
#include "debug.h"
#include "globals.h"
#include "model.h"
#include "requests.h"
#include "servers.h"
#include "ui.h"

#define NUM_DISP_SENSORS	4	

// Private Stuff
// ----------------------------------------------------------------------------

typedef struct {
	int time;
	int idx;
} SensorData;

typedef struct {
	SensorData 	sensorsBuf[NUM_DISP_SENSORS]; 
	RB  	   	sensors;
	TrackModel 	model;
	TID 		ios2Tid;
	int			train1Id;
	int			train2Id;
	int 		trackId;
} UI;

// Initialize the UI
void ui_init(UI *ui);
// Save the 2 train Ids in the UI
void ui_registerTrains( UI *ui );

// Save teh cursor position
void ui_saveCursor(UI *ui);
// Restore the cursor position
void ui_restoreCursor(UI *ui);
// Clear the screen
void ui_clearScreen( UI *ui );
// Draw a splash screen
void ui_splashScreen( UI *ui );

// Draw the track map on the screen
void ui_drawMap (UI *ui);
// Update a last hit sensor information
void ui_updateSensor( UI *ui, int idx, int time );
// Update a changed switch / sensor on the map
void ui_updateMap( UI *ui, int idx, int state );
// Display the current time at a particular location on the screen
void ui_displayTimeAt( UI *ui, int x, int y, int time );
// Update a train's location on the screen
void ui_updateTrainLocation( UI *ui, int idx, int dist, int trainId );

// Display a string on the screen at the given location in the given colour
void strPrintAt (int ios2tid, int x, int y, char *str, 
				 ForeColour fc, BackColour bc);
// Display an integer a the locaiton given the format and colours
void intPrintAt (int ios2Tid, int x, int y, char *fmt, int value, 
				 ForeColour fc, BackColour bc);

// A Notifying task that displays the the current time in the UI
void uiclk_run();

// ----------------------------------------------------------------------------

void ui_run () {
	UI  ui;
	int senderTid;
	UIRequest req;

	// Initialize the UI
	ui_init( &ui );

	// Get the information about the 2 trains
	ui_registerTrains( &ui );

	FOREVER {
		// Receive a message
		Receive( &senderTid, (char*)&req, sizeof(UIRequest) );

		// Reply immediately
		Reply  ( senderTid, (char*)&senderTid, sizeof(int) );

		// Save the shell's cursor location
		ui_saveCursor(&ui);

		// Display the information at the correct location
		switch( req.type ) {
			case CLOCK:
				ui_displayTimeAt( &ui, 58, 8, req.time );
				break;
			
			case TRACK_SERVER:
				ui_updateMap( &ui, req.idx, req.state );
				break;

			case DETECTIVE:
				ui_updateSensor( &ui, req.idx, req.time );
				break;

			case TRAIN:
				ui_updateTrainLocation( &ui, req.idx, req.dist, req.trainId );
				break;
			default:
				// TODO: error?
				break;
		}

		// Restore the location of the cursor for the shell
		ui_restoreCursor(&ui);
	}
	Exit();	// This will never be called.
}

void ui_saveCursor(UI *ui) {
	cprintf (ui->ios2Tid, "\0337");
}

void ui_restoreCursor (UI *ui) {
	cprintf (ui->ios2Tid, "\0338");
}

void ui_init (UI *ui) {
	debug ("ui_init\r\n");
	char ch;
	int  shellTid;
	int	 err = NO_ERROR;
	int  uiclkTid;
	int i;

	// Init private members
	ui->ios2Tid = WhoIs(SERIALIO2_NAME);
	
	rb_init (&(ui->sensors), ui->sensorsBuf );
	for ( i = 0; i < NUM_DISP_SENSORS; i ++ ) {
		ui->sensorsBuf[i].time = 0;
		ui->sensorsBuf[i].idx  = -1;
	}
	
	// Get the model from the shell
	Receive(&shellTid, (char*)&ch, sizeof(ch));
	if ( ch != 'A' && ch != 'a' && ch != 'B' && ch != 'b' ) {
		err = INVALID_TRACK;
	}
	
	//ui_clearScreen( ui->ios2Tid );
	//ui_splashScreen(ui->ios2Tid);
	
	// Parse the model
	ui->trackId = ( ch == 'A' || ch == 'a' ) ? TRACK_A : TRACK_B;
	parse_model( ui->trackId, &ui->model );
	
	// Start Drawing the ui
	ui_clearScreen( ui );

	// Limit the scroll range of the shell
	cprintf( ui->ios2Tid , "\033[20;24r" );
	// Turn off cursor
	cprintf( ui->ios2Tid, "\033[?25l");

	// Draw the map on the screen
	ui_drawMap( ui );
		
	strPrintAt (ui->ios2Tid, 22, 7 ,  "   Train1 Data   ", CYAN_FC, WHITE_BC);
	strPrintAt (ui->ios2Tid, 22, 8 ,  " Last Hit        ", CYAN_FC, WHITE_BC);
	strPrintAt (ui->ios2Tid, 22, 9 ,  " Sensor  :       ", CYAN_FC, WHITE_BC);
	strPrintAt (ui->ios2Tid, 22, 10 , " Dist(mm):       ", CYAN_FC, WHITE_BC);

	strPrintAt (ui->ios2Tid, 55, 7 ,  "   Train2 Data   ", CYAN_FC, WHITE_BC);
	strPrintAt (ui->ios2Tid, 55, 8 ,  " Last Hit        ", CYAN_FC, WHITE_BC);
	strPrintAt (ui->ios2Tid, 55, 9 ,  " Sensor  :       ", CYAN_FC, WHITE_BC);
	strPrintAt (ui->ios2Tid, 55, 10 , " Dist(mm):       ", CYAN_FC, WHITE_BC);
	
	strPrintAt (ui->ios2Tid, 5, 9, "Time:", CYAN_FC, WHITE_BC);
	strPrintAt (ui->ios2Tid, 1, 19, "Sensors:", CYAN_FC, BLACK_BC);

	// CREATE THE TIMER NOTIFIER
	uiclkTid = Create( OTH_NOTIFIER_PRTY, &uiclk_run );
	Send( uiclkTid, (char*)&ch, sizeof(char),
					(char*)&ch, sizeof(char) );

	// REGISTER WITH THE NAME SERVER
	RegisterAs( UI_NAME );
	
	// Reply to the shell
	Reply  (shellTid,  (char*)&err, sizeof(int));

}

void ui_registerTrains( UI *ui ) {
	int shellTid;
	int trainId;

	Receive(&shellTid, (char*)&trainId, sizeof(int));
	Reply  (shellTid,  (char*)&trainId, sizeof(int));
	
	ui->train1Id = trainId;

	Receive(&shellTid, (char*)&trainId, sizeof(int));
	Reply  (shellTid,  (char*)&trainId, sizeof(int));
	
	ui->train2Id = trainId;
}

void ui_updateSensor( UI *ui, int idx, int time ) {
	char  bank[4];
	char  num;
	int   i;
	int   x[] = {10, 23, 36, 49};

	SensorData data = { time, idx };
	
	// Store the new data in the buffer
	rb_force ( &ui->sensors, &data );
	
	for ( i = 0; i < NUM_DISP_SENSORS; i ++ ) {
		if ( ui->sensorsBuf[i].idx == -1 ) {
			return;
		}

		ui_displayTimeAt( ui, x[i], 19,
						  ui->sensorsBuf[i].time/(100/ MS_IN_TICK) );
		
		bank[0] = sensor_bank( ui->sensorsBuf[i].idx );
		bank[1] = ' ';
		bank[2] = ' ';
		bank[3] = '\0';
		
		strPrintAt( ui->ios2Tid, x[i]+8, 19, bank,
					   WHITE_FC, BLACK_BC );

		num = sensor_num ( ui->sensorsBuf[i].idx );
		
		intPrintAt( ui->ios2Tid, x[i]+9, 19, "%d", num,
					   WHITE_FC, BLACK_BC );
	}
}

void ui_updateMap( UI* ui, int idx, int state ) {
	if ( idx == 153 ) {
		idx = 22;
	} else if ( idx == 154 ) {
		idx = 19;
	} else if ( idx == 155 ) {
		idx = 20;
	} else if ( idx == 156 ) {
		idx = 21;
	}

	Node *n = &ui->model.nodes[idx+39]; // you get a switch idx
	char  ch[] = "\033(0 \033(B";

	// Curved
	if ( state == 1 ) {
		if ( idx == 1 || idx == 2 || idx == 3 || idx == 4 || idx == 14 ) {
			ch[6] = 'q';
		} else if ( idx == 5 || idx == 7 || idx == 8 || idx == 16 ) {
			ch[6] = 'j';
		} else if ( idx == 6 || idx == 9 || idx == 13 ) {
			ch[6] = 'k';
		} else if ( idx == 10 || idx == 11 || idx == 12 ) {
			ch[6] = 'l';
		} else {
			ch[6] = 'm';
		}
	} else {
		if ( idx == 1 || idx == 2 || idx == 3 ) {
			ch[6] = 'm';
		} else if ( idx == 4 ) {
			ch[6] = 'l';
		} else if ( idx == 14 ) {
			ch[6] = 'j';
		} else if ( idx == 5 || idx == 6 || idx == 7 || idx == 10 ||
					idx == 11 || idx == 12 || idx == 13 || idx == 16 ||
					idx == 17 || idx == 18 ) {
			ch[6] = 'q';
		} else  {
			ch[6] = 'x';
		}
	}

	debug ("name:%s idx:%d newST:%s\r\n", n->name, idx, ch);
	switch (n->type) {
		case NODE_SWITCH:
			strPrintAt( ui->ios2Tid, 
						   n->x, 
						   n->y, 
						   ch,
						   BLUE_FC, GREEN_BC);
			break;
		case NODE_STOP:
		case NODE_SENSOR:
			// Do nothing.
			break;
	}
}

void ui_updateTrainLocation( UI *ui, int idx, int dist, int trainId ) {
	int  num = sensor_num ( idx );
	char bank[6];
	bank[0] = ' ';
	bank[1] = ' ';
	bank[2] = ' ';
	bank[3] = ' ';
	bank[4] = ' ';
	bank[5] = '\0';

	// Determine the x-coord
	int x = (trainId == ui->train1Id) ? 34 : 43;

	// Clear distance
	strPrintAt( ui->ios2Tid, x, 10, bank,
				   BLUE_FC, WHITE_BC );

	bank[0] = sensor_bank( idx );
	strPrintAt( ui->ios2Tid, x, 9, bank,
				   BLUE_FC, WHITE_BC );
	
	intPrintAt( ui->ios2Tid, x+1, 9, "%d", num,
				   BLUE_FC, WHITE_BC );
	
	intPrintAt( ui->ios2Tid, x, 10, "%d", dist,
				   BLUE_FC, WHITE_BC );
}

void ui_displayTimeAt ( UI *ui, int x, int y, int time ) {
	int ios2Tid = ui->ios2Tid;
	int tens, mins, secs;
	tens = time % 10;
	secs = (time / 10) % 60;
	mins = time / 600;
	
	intPrintAt (ios2Tid, x,   y, "%02d:", mins, CYAN_FC, BLACK_BC);
	intPrintAt (ios2Tid, x+3, y, "%02d:", secs, CYAN_FC, BLACK_BC);
	intPrintAt (ios2Tid, x+6, y, "%01d",  tens, CYAN_FC, BLACK_BC);
}

void ui_clearScreen( UI *ui ) {
	cprintf(ui->ios2Tid, "\033c");
}

void strPrintAt (int ios2Tid, int x, int y, char *str, 
				 ForeColour fc, BackColour bc) {

//	cprintf( ios2Tid, "\033[%dm\033[%dm\033[%d;%dH%s\033[%dm\033[%dm",
//			 fc, bc,
//			 y, x,
//			 str, DEFAULT_FC, DEFAULT_BC );

//  cprintf (ios2Tid, "\033[%dm", bc);
//	cprintf (ios2Tid, "\033[%dm", bc);
/*	cprintf( ios2Tid, "\033[%d;%dH%s\033[%dm\033[%dm", 
			 y, x, str, DEFAULT_FC, DEFAULT_BC );*/

	// Set the colour
	cprintf( ios2Tid, "\033[%dm\033[%dm", fc, bc );

	// Move the cursor
	cprintf (ios2Tid, "\033[%d;%dH", y, x);

	// Write the character
	cprintf (ios2Tid, "%s", str);

	// Reset the colour
	cprintf (ios2Tid, "\033[%dm", DEFAULT_FC);
	cprintf (ios2Tid, "\033[%dm", DEFAULT_BC);
}

void intPrintAt (int ios2Tid, int x, int y, char *fmt, int value, 
				 ForeColour fc, BackColour bc) {

	// Set the colour
	cprintf (ios2Tid, "\033[%dm\033[%dm\033[%d;%dH", fc, bc, y, x);
//	cprintf (ios2Tid, "\033[%dm", bc);

	// Move the cursor
//	cprintf (ios2Tid, "\033[%d;%dH", y, x);

	// Write the character
	cprintf (ios2Tid, fmt, value);

	// Reset the colour
	cprintf (ios2Tid, "\033[%dm\033[%dm", DEFAULT_FC, DEFAULT_BC);
//	cprintf (ios2Tid, "\033[%dm", DEFAULT_BC);
}

void ui_splashScreen( UI *ui ) {
char frog[][100] = {
"				     _    _", 	        
"                    / \\  / \\",              
"                   |@|  |@|",             
"                   _\\_/__\\_/_",              
"                  /          \\",                  
"           __    /            \\    __",        
"          /  \\   \\ \\        / /   /  \\",           
"         /    \\  /\\ \\______/ /\\  /    \\",          
"        /      \\/  \\________/  \\/      \\",            
"       /   /\\  /   /        \\   \\  /\\   \\",           
"      /   /  \\/   /          \\   \\/  \\   \\",          
" ____/   /    \\   \\          /   /    \\   \\____",     
"/___    /      \\   \\        /   /      \\    ___\\",    
"   / /\\ \\      /    \\______/    \\      / /\\ \\",       
"  / /  \\/     /      \\    /      \\     \\/  \\ \\",      
"  \\/         / /\\  /\\ \\  / /\\  /\\ \\         \\/",      
"             \\/  \\/  \\/  \\/  \\/  \\/                 " };

    int i;
    for ( i = 0; i < 20; i ++ ) {
        strPrintAt( ui->ios2Tid, 1, i+20,
        		    (char*)frog[i], GREEN_FC, BLACK_BC );
	}

}

void ui_drawMap( UI *ui ) {
	//TODO: Due to terminal issue!
	Delay( 200 / MS_IN_TICK, WhoIs(CLOCK_NAME) );
	
	char mapA[][90] = {	
"tqqqqqqqqqqqqqwqqqqqqqqqqqwqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqk    ",
"             lj        lqqj                                            mk   ",
"tqqqqqqqqqwqqj     lqqqvqqqqqqqqqqqqwqqqqqqqqqqqqqwqqqqqqqqqqqqqqqk     mk  ",
"         lj       lj                mk           lj               mk     x  ",
"tqqqqqqqqj       lj                  mk    w    lj                 mk    x  ",
"                lj                    mk   x   lj                   mqk  x  ",
"              lqj                      mqk x lqj                      mqqn  ",
"              x                          mqnqj                           x  ",
"              x                          lqnqk                           x  ",
"              mqk                      lqj x mqk                      lqqn  ",
"                mk                    lj   x   mk                   lqj  x  ",
"tqqqqqqqk        mk                  lj    v    mk                 lj    x  ",
"        mk        mk                lj           mk               lj     x  ",
"tqqqqqqqqvqqk      mqqqwqqqqqqqqqqqqvqqqqqqqqqqqqqvqqqqqqqqqqqqqqqj     lj  ",
"            mk         mk                                              lj   ",
"tqqqqqqqqqqqqvqqk       mqqqwqqqqqqqqqqqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqqqj    ",
"                mk          mqqk                      lqqj                  ",
"tqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqu         ",
"                                                                            "};
    	char mapB[][90] = {	
"tqqqqqqqqqqqqwqqqqqqqqqqqqwqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqk    ",
"            lj         lqqj                                            mk   ",
"tqqqqqqqqwqqj      lqqqvqqqqqqqqqqqqwqqqqqqqqqqqqqwqqqqqqqqqqqqqqqk     mk  ",
"        lj        lj                mk           lj               mk     x  ",
"       lj        lj                  mk    w    lj                 mk    x  ",
"      lj        lj                    mk   x   lj                   mqk  x  ",
"     lj       lqj                      mqk x lqj                      mqqn  ",
"     x        x                          mqnqj                           x  ",
"     x        x                          lqnqk                           x  ",
"     mk       mqk                      lqj x mqk                      lqqn  ",
"      mk        mk                    lj   x   mk                   lqj  x  ",
"       mk        mk                  lj    v    mk                 lj    x  ",
"        mk        mk                lj           mk               lj     x  ",
"tqqqqqqqqvqqk      mqqqwqqqqqqqqqqqqvqqqqqqqqqqqqqvqqqqqqqqqqqqqqqj     lj  ",
"            mk         mk                                              lj   ",
"tqqqqqqqqqqqqvqqk       mqqqwqqqqqqqqqqqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqqqj    ",
"                mk          mqqk                      lqqj                  ",
"tqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqu         ",
"                                                                            "};
	int  i;
	if ( ui->trackId == TRACK_A ) {
		for ( i = 0; i < 18; i ++ ) {
			cprintf( ui->ios2Tid, "\033(0" );
			strPrintAt( ui->ios2Tid, 3, i+1, mapA[i], BLACK_FC, GREEN_BC );
			cprintf( ui->ios2Tid, "\033(B" );
		}
	} else {
		for ( i = 0; i < 18; i ++ ) {
			cprintf( ui->ios2Tid, "\033(0" );
			strPrintAt( ui->ios2Tid, 3, i+1, mapB[i], BLACK_FC, GREEN_BC );
			cprintf( ui->ios2Tid, "\033(B" );
		}
	}
}

// ----------------------------------------------------------------------------

void uiclk_run () {
	char 		ch;
	int			clockTid = WhoIs (CLOCK_NAME);
	int 		uiTid;
	UIRequest 	req;

	Receive( &uiTid, (char*)&ch, sizeof(char) );
	Reply  ( uiTid,  (char*)&ch, sizeof(char) );
	
	req.type = CLOCK;
	
	FOREVER {
		// Wait a bit
		Delay( 100 / MS_IN_TICK, clockTid );
		// Grab the new time
		req.time = Time( clockTid ) / (100 / MS_IN_TICK);
		
		// Tell the server!
		Send( uiTid, (char*)&req,	   sizeof(UIRequest),
			  		 (char*)&req.time, sizeof(int) );
	}

	Exit(); // This will never be called
}
