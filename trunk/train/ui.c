/*
 * User Interface for the train projet
 * becmacdo
 * dgoc
 */
#define DEBUG 1

#include <string.h>
#include <math.h>

#include "buffer.h"
#include "debug.h"
#include "globals.h"
#include "model.h"
#include "requests.h"
#include "servers.h"
#include "ui.h"

#define NUM_DISP_SENSORS	4	
#define NUM_POINTS			30	

// Private Stuff
// ----------------------------------------------------------------------------

typedef struct {
	int time;
	int idx;
} SensorData;

typedef struct {
	int x;
	int y;
	char display[2];
} SwitchData;

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
void ui_updateMap( UI* ui, int idx, SwitchDir state );
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
				ui_displayTimeAt( &ui, 46, 2, req.time );
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
	cprintf (ui->ios2Tid, "\033[?25l\0337");
}

void ui_restoreCursor (UI *ui) {
	cprintf (ui->ios2Tid, "\0338\033[?25h");
}

void ui_init (UI *ui) {
	debug ("ui_init\r\n");
	char ch;
	int  shellTid;
	int	 err = NO_ERROR;
	int i;

	// Init private members
	ui->ios2Tid = WhoIs(SERIALIO2_NAME);
	ui->train1Id = -1;
	ui->train2Id = -1;
	
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
		
	strPrintAt (ui->ios2Tid, 22, 7 ,  "   Train1 Data   ", RED_FC, WHITE_BC);
	strPrintAt (ui->ios2Tid, 22, 8 ,  " Last Hit        ", RED_FC, WHITE_BC);
	strPrintAt (ui->ios2Tid, 22, 9 ,  " Sensor  :       ", RED_FC, WHITE_BC);
	strPrintAt (ui->ios2Tid, 22, 10 , " Dist(mm):       ", RED_FC, WHITE_BC);

	strPrintAt (ui->ios2Tid, 53, 7 ,  "   Train2 Data   ", RED_FC, WHITE_BC);
	strPrintAt (ui->ios2Tid, 53, 8 ,  " Last Hit        ", RED_FC, WHITE_BC);
	strPrintAt (ui->ios2Tid, 53, 9 ,  " Sensor  :       ", RED_FC, WHITE_BC);
	strPrintAt (ui->ios2Tid, 53, 10 , " Dist(mm):       ", RED_FC, WHITE_BC);
	
//	strPrintAt (ui->ios2Tid, 41, 2, "Time:", CYAN_FC, WHITE_BC);
	strPrintAt (ui->ios2Tid, 1, 19, "Sensors:", WHITE_FC, BLACK_BC);

	// CREATE THE TIMER NOTIFIER
//	uiclkTid = Create( OTH_NOTIFIER_PRTY, &uiclk_run );
//	Send( uiclkTid, (char*)&ch, sizeof(char),
//					(char*)&ch, sizeof(char) );

	// REGISTER WITH THE NAME SERVER
	RegisterAs( UI_NAME );
	
	// Reply to the shell
	debug ("Replying to the shell\r\n");
	Reply  (shellTid,  (char*)&err, sizeof(int));
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

void ui_updateMap( UI* ui, int idx, SwitchDir state ) {
	
	if ( idx >= 153 ) {
		idx -= 134;
	}
	idx --;


	char  ch[] = "\033(0q\033(B";

	SwitchData switches[] ={
		{12,	14,	{'m',	'q'}},	// SW1
		{16,	16,	{'m',	'q'}},	// SW2
		{20,	18,	{'q',	'm'}},	// SW3
		{12,	3,	{'l',	'q'}},	// SW4
		{57,	18,	{'q',	'j'}},	// SW5
		{32,	16,	{'q',	'k'}},	// SW6
		{60,	16,	{'q',	'l'}},	// SW7
		{75,	11,	{'x',	'j'}},	// SW8
		{75,	6,	{'x',	'k'}},	// SW9
		{53,	3,	{'q',	'l'}},	// SW10
		{27,	1,	{'q',	'l'}},	// SW11
		{16,	1,	{'q',	'l'}},	// SW12
		{41,	3,	{'q',	'k'}},	// SW13
		{24,	3,	{'j',	'q'}},	// SW14
		{24,	14,	{'k',	'q'}},	// SW15
		{41,	14,	{'q',	'j'}},	// SW16
		{53,	14,	{'q',	'm'}},	// SW17
		{35,	18,	{'q',	'm'}},	// SW18
		{47,	10,	{'x',	'j'}},	// SW99
		{47,	9,	{'x',	'm'}},	// SW9A
		{47,	7,	{'x',	'l'}},	// SW9B
		{47,	8,	{'x',	'k'}}};	// SW9C
	
	if( idx < 0 && idx >= array_size( switches ) ) return; // fail silently
	assert( state >= 0 && state < 2 );

	ch[3] = switches[idx].display[state];

//	debug ("name:%s idx:%d newST:%s\r\n", n->name, idx, ch);
	strPrintAt( ui->ios2Tid, 
					switches[idx].x, 
					switches[idx].y, 
					ch,
					RED_FC, BLACK_BC);
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

	// Register the train with the UI.
	// We neeed to do more for > 2 trains.
	if ( ui->train1Id == -1 ) {
		ui->train1Id = trainId;
	}

	// Determine the x-coord
	int x = (trainId == ui->train1Id) ? 34 : 64;

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
"tqqqqqqqqqqqqwqqqqqqqqqqwqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqk      ",
"            lj       lqqj                                            mqk    ",
"tqqqqqqqqwqqj       lvqqqqqqqqqqqqqqqqwqqqqqqqqqqqwqqqqqqqqqqqqqqqqqk  mk   ",
"        lj        lqj                 mqk       lqj                 mqk x   ",
"tqqqqqqqj        lj                     mk  w  lj                     mkx   ",
"                lj                       mk x lj                       mu   ",
"               lj                         mktqj                         mk  ",
"               x                           mu                            x  ",
"               x                            tk                           x  ",
"               mk                         lqumk                         lj  ",
"                mk                       lj x mk                       lu   ",
"tqqqqqqqk        mk                     lj  v  mk                     ljx   ",
"        mk        mqk                 lqj       mqk                 lqj x   ",
"tqqqqqqqqvqqk       mwqqqqqqqqqqqqqqqqvqqqqqqqqqqqvqqqqqqqqqqqqqqqqqj  lj   ",
"            mk       mqqk                                            lqj    ",
"tqqqqqqqqqqqqvqqk       mqqqqwqqqqqqqqqqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqj      ",
"                mk           mqqk                     lqqj                  ",
"tqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqu   ",
"                                                                            "};
   	char mapB[][90] = {	
"tqqqqqqqqqqqqwqqqqqqqqqqwqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqk      ",
"            lj       lqqj                                            mqk    ",
"tqqqqqqqqwqqj       lvqqqqqqqqqqqqqqqqwqqqqqqqqqqqwqqqqqqqqqqqqqqqqqk  mk   ",
"        lj        lqj                 mqk       lqj                 mqk x   ",
"       lj        lj                     mk  w  lj                     mkx   ",
"      lj        lj                       mk x lj                       mu   ",
"     lj        lj                         mktqj                         mk  ",
"     x         x                           mu                            x  ",
"     x         x                            tk                           x  ",
"     mk        mk                         lqumk                         lj  ",
"      mk        mk                       lj x mk                       lu   ",
"       mk        mk                     lj  v  mk                     ljx   ",
"        mk        mqk                 lqj       mqk                 lqj x   ",
"tqqqqqqqqvqqk       mwqqqqqqqqqqqqqqqqvqqqqqqqqqqqvqqqqqqqqqqqqqqqqqj  lj   ",
"            mk       mqqk                                            lqj    ",
"tqqqqqqqqqqqqvqqk       mqqqqwqqqqqqqqqqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqj      ",
"                mk           mqqk                     lqqj                  ",
"tqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqu   ",
"                                                                           "};
	int  i;
	if ( ui->trackId == TRACK_A ) {
		for ( i = 0; i < 18; i ++ ) {
			cprintf( ui->ios2Tid, "\033(0\033[1m" );
			strPrintAt( ui->ios2Tid, 3, i+1, mapA[i], YELLOW_FC, BLACK_BC );
			cprintf( ui->ios2Tid, "\033(B\033[0m" );
		}
	} else {
		for ( i = 0; i < 18; i ++ ) {
			cprintf( ui->ios2Tid, "\033(0\033[1m" );
			strPrintAt( ui->ios2Tid, 3, i+1, mapB[i], YELLOW_FC, BLACK_BC );
			cprintf( ui->ios2Tid, "\033(B\033[0m" );
		}
	}
}

// ----------------------------------------------------------------------------

void uiclk_run () {
	char 		ch;
	int			clockTid = WhoIs (CLOCK_NAME);
	int 		uiTid;
	UIRequest 	req;

	Receive( &uiTid, &ch, sizeof(char) );
	Reply  ( uiTid,  &ch, sizeof(char) );
	
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
