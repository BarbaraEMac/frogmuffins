/*
 * User Interface for the train projet
 * becmacdo
 * dgoc
 */
#define DEBUG 2

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
} UI;

void ui_init(UI *ui);
void ui_clearScreen (int ios2Tid);
void ui_splashScreen(int ios2Tid);

void ui_drawMap (int ios2Tid, TrackModel *model);
void ui_updateSensor( UI *ui, int idx, int time );
void ui_displayPrompt( int ios2Tid, char *fmt, char *str );
void ui_updateMap( UI *ui, int idx, int state );
void ui_displayTimeAt( int ios2Tid, int x, int y, int time );

void ui_updateTrainLocation( UI *ui, int idx, int dist);

void ui_strPrintAt (int ios2tid, int x, int y, char *str, 
				 ForeColour fc, BackColour bc);
void ui_intPrintAt (int ios2Tid, int x, int y, char *fmt, int value, 
				 ForeColour fc, BackColour bc);

void uiclk_run();

// ----------------------------------------------------------------------------

void ui_run () {
	UI  ui;
	int senderTid;
	UIRequest req;
	char ch;

	// Initialize the UI
	ui_init (&ui);

	// Turn off cursor
	cprintf (ui.ios2Tid, "\033[?25l");
			
	FOREVER {
		// Receive a message
		Receive( &senderTid, (char*)&req, sizeof(UIRequest) );
//		debug ("ui: received from %d\r\n", senderTid);

		// Reply immediately
		Reply  ( senderTid, (char*)&senderTid, sizeof(int) );

		// Display the information at the correct location
		switch( req.type ) {
			case CLOCK:
				//ui_displayTimeAt( ui.ios2Tid, 65, 7, time );
				break;
			
			case TRACK_SERVER:
				debug ("updating hte map\r\n");
				ui_updateMap( &ui, req.idx, req.state );
				break;

			case SHELL:
				ui_displayPrompt( ui.ios2Tid, req.fmt, req.str );
				break;
			
			case DETECTIVE:
				ui_updateSensor( &ui, req.idx, req.time );
				break;

			case TRAIN:
				ui_updateTrainLocation( &ui, req.idx, req.dist );
				break;
		}
	}
	Exit();	// This will never be called.
}

void ui_init (UI *ui) {
	debug ("ui_init\r\n");
	char ch;
	int  shellTid;
	int  track;
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
	
	//ui_splashScreen(ui->ios2Tid);
	
	// Get the model from the shell
	Receive(&shellTid, (char*)&ch, sizeof(ch));
	if ( ch != 'A' && ch != 'a' && ch != 'B' && ch != 'b' ) {
		err = INVALID_TRACK;
	}
	
	// Parse the model
	track = ( ch == 'A' || ch == 'a' ) ? TRACK_A : TRACK_B;
	parse_model( track, &ui->model );
	
	// Start Drawing the ui
	ui_clearScreen( ui->ios2Tid );

	// Draw the map on the screen
	ui_drawMap( ui->ios2Tid, &ui->model );
	
	ui_strPrintAt (ui->ios2Tid, 1, 20, "Sensors: ", RED_FC, WHITE_BC);
	ui_strPrintAt (ui->ios2Tid, 1, 21, "Prompt: ", BLUE_FC, BLACK_BC);

	// CREATE THE TIMER NOTIFIER
//	uiclkTid = Create( OTH_NOTIFIER_PRTY, &uiclk_run );
//	Send( uiclkTid, (char*)&ch, sizeof(char),
//					(char*)&ch, sizeof(char) );

	// REGISTER WITH THE NAME SERVER
	RegisterAs( UI_NAME );
	
	// Reply to the shell
	Reply  (shellTid,  (char*)&err, sizeof(int));
}

void ui_updateSensor( UI *ui, int idx, int time ) {
	char  bank[2];
	char  num;
	int   i;
	int   x[] = {10, 23, 36, 49};

	SensorData data = { time, idx };
	
	// Store the new data in the buffer
	rb_force ( &ui->sensors, &data );
	
	// Erase to the right
	cprintf( ui->ios2Tid, "\033[20;1H\033[0K");

	for ( i = 0; i < NUM_DISP_SENSORS  - 1; i ++ ) {
		if ( ui->sensorsBuf[i].idx == -1 ) {
			return;
		}

		ui_displayTimeAt( ui->ios2Tid, x[i], 20,
						  ui->sensorsBuf[i].time );
		
		bank[0] = sensor_bank( ui->sensorsBuf[i].idx );
		bank[1] = '\0';
		
		ui_strPrintAt( ui->ios2Tid, x[i]+8, 20, bank,
					   WHITE_FC, BLACK_BC );

		num = sensor_num ( ui->sensorsBuf[i].idx );
		
		ui_intPrintAt( ui->ios2Tid, x[i]+9, 20, "%d", num,
					   WHITE_FC, BLACK_BC );
	}
}

void ui_displayPrompt( int ios2Tid, char *fmt, char *str ) {
	cprintf(ios2Tid, "\033[36m");
	cprintf(ios2Tid, "\033[40B");
	cprintf(ios2Tid, fmt, str);
	cprintf(ios2Tid, "\033[37m");
}

void ui_updateMap( UI* ui, int idx, int state ) {
	Node *n = &ui->model.nodes[idx+39]; // you get a switch idx
	char  ch[2];

	ch[0] = (state == 1) ? 'C' : 'S';
	ch[1] = '\0';

//	debug ("name:%s idx:%d newST:%s\r\n", n->name, idx, ch);
	switch (n->type) {
		case NODE_SWITCH:
			ui_strPrintAt( ui->ios2Tid, 
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

void ui_updateTrainLocation( UI *ui, int idx, int dist ) {
//	printf( "\033[15;30H%c%d:%dmm\033[24;0H", 
//			sensor_bank( idx ),
//			sensor_num( idx ), dist );
}

void ui_displayTimeAt ( int ios2Tid, int x, int y, int time ) {
	int tens, mins, secs;
	tens = time % 10;
	secs = (time / 10) % 60;
	mins = time / 600;
	
	ui_intPrintAt (ios2Tid, x,   y, "%02d:", mins, RED_FC, BLACK_BC);
	ui_intPrintAt (ios2Tid, x+3, y, "%02d:", secs, RED_FC, BLACK_BC);
	ui_intPrintAt (ios2Tid, x+6, y, "%01d",  tens, RED_FC, BLACK_BC);
}

void ui_clearScreen (int ios2Tid) {
	cprintf(ios2Tid, "\033c");
}

void ui_strPrintAt (int ios2Tid, int x, int y, char *str, 
				 ForeColour fc, BackColour bc) {

	// Set the colour
	cprintf( ios2Tid, "\033[%dm\033[%dm\033[%d;%dH%s\033[%dm\033[%dm", 
			 fc, bc, y, x, str, DEFAULT_FC, DEFAULT_BC );

//	cprintf (ios2Tid, "\033[%dm", bc);

	// Move the cursor
//	cprintf (ios2Tid, "\033[%d;%dH", y, x);

	// Write the character
//	cprintf (ios2Tid, "%s", str);

	// Reset the colour
//	cprintf (ios2Tid, "\033[%dm", DEFAULT_FC);
//	cprintf (ios2Tid, "\033[%dm", DEFAULT_BC);
}

void ui_intPrintAt (int ios2Tid, int x, int y, char *fmt, int value, 
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

void ui_splashScreen(int ios2Tid) {
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
        ui_strPrintAt( ios2Tid, 1, i+5,
        		    (char*)frog[i], GREEN_FC, BLACK_BC );
	}

}

void ui_drawMap (int ios2Tid, TrackModel *model) {
	//TODO: Due to terminal issue!
	Delay( 200 / MS_IN_TICK, WhoIs(CLOCK_NAME) );
	char mapA[][90] = {	
"tqqqqqqqqqqqqwqqqqqqqqqqqqwqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqk    ",
"             x         lqqj                                            x    ",
"tqqqqqqqqwqqqj     lqqqvqqqqqqqqqqqqqwqqqqqqqqqqqqqwqqqqqqqqqqqqqqqk   x    ",
"         x         x                 x             x               x   mqk  ",
"tqqqqqqqqj         x                 x             x               mqk   x  ",
"              lqqqqj                 mqqk       lqqj                 mqqqvk ",
"              x                         x   w   x                         x ",
"              x                         mqk x lqj                         x ",
"              x                           mqnqj                           x ",
"              x                           lqnqk                           x ",
"              x                         lqj x mqk                         x ",
"              x                         x   v   x                         x ",
"              mqqqqk                 lqqj       mqqk                 lqqqwj ",
"tqqqqqqqqk         x                 x             x               lqj   x  ",
"         x         x                 x             x               x   lqj  ",
"tqqqqqqqqvqqqk     mqqqwqqqqqqqqqqqqqvqqqqqqqqqqqqqvqqqqqqqqqqqqqqqj   x    ",
"             x         x                                               x    ",
"tqqqqqqqqqqqqvqqqk     mqqqqwqqqqqqqqqqqqqqqqqqqqqqqqqqqqwqqqqqqqqqqqqqj    ",
"                 x          mqqk                      lqqj                  ",
"tqqqqqqqqqqqqqqqqvqqqqqqqqqqqqqvqqqqqqqqqqqqqqqqqqqqqqvqqqqqqqqqqqu         ",
"                                                                            "};
    int  i;

	for ( i = 0; i < 20; i ++ ) {
		cprintf (ios2Tid, "\033(0");
		ui_strPrintAt (ios2Tid, 3, i+1, mapA[i], BLACK_FC, GREEN_BC);
		cprintf (ios2Tid, "\033(B");
	}
	
	for ( i = 0; i < model->num_nodes; i ++ ) {
		char ch[2];
		ch[1] = '\0';

		switch (model->nodes[i].type) {
			case NODE_STOP:
			case NODE_SENSOR:
				break;
			case NODE_SWITCH:
				*ch = 'S';
				ui_strPrintAt (ios2Tid, 
					model->nodes[i].x, 
					model->nodes[i].y, 
					ch,
					BLUE_FC, GREEN_BC);
				break;
		}
	}
}


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
		req.time = Time( clockTid );
		
		// Tell the server!
		Send( uiTid, (char*)&req,	   sizeof(UIRequest),
			  		 (char*)&req.time, sizeof(int) );
	}

	Exit(); // This will never be called
}
