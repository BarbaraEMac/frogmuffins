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

#define NUM_DISP_SENSORS		5

// Private Stuff
// ----------------------------------------------------------------------------

typedef struct {
	int sensorsBuf[NUM_DISP_SENSORS]; // High 15 bits = idx ; low 15 bits = time
	RB  sensors;
	TrackModel model;
	int ios2Tid;
} UI;

void ui_init(UI *ui);
void ui_clearScreen (int ios2Tid);
void ui_splashScreen(int ios2Tid);

void ui_drawMap (int ios2Tid, TrackModel *model);
void ui_updateSensor( UI *ui, int idx, int time );
void ui_displayPrompt( int ios2Tid, char *fmt, char *str );
void ui_updateMap( UI *ui, int idx, int state );
void ui_displayTime( int ios2Tid, int time );

void ui_updateTrainLocation( UI *ui, int idx, int dist);

void ui_strPrintAt (int ios2tid, int x, int y, char *str, 
				 ForeColour fc, BackColour bc);
void ui_intPrintAt (int ios2Tid, int x, int y, char *fmt, int value, 
				 ForeColour fc, BackColour bc);

void uiclk_run();

// ----------------------------------------------------------------------------

void ui_run () {
	debug ("ui: run\r\n");
	UI  ui;
	int senderTid;
	UIRequest req;
	char ch;
	

	// Initialize the UI
	ui_init (&ui);

	
			
	FOREVER {
		// Receive a message
		Receive( &senderTid, (char*)&req, sizeof(UIRequest) );
		// Reply immediately
		Reply  ( senderTid, (char*)&senderTid, sizeof(int) );

		// Display the information at the correct location
		switch( req.type ) {
			case CLOCK:
				ui_displayTime( ui.ios2Tid, req.time );
				break;
			
			case TRACK_SERVER:
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
	char ch;
	int  shellTid;
	int  track;
	int	 err = NO_ERROR;
	int  uiclkTid;

	ui->ios2Tid = WhoIs(SERIALIO2_NAME);
	//ui_splashScreen(ui->ios2Tid);
	
	// Get the model from the shell
	Receive(&shellTid, (char*)&ch, sizeof(ch));
	if ( ch != 'A' && ch != 'a' && ch != 'B' && ch != 'b' ) {
		err = INVALID_TRACK;
	}
	Reply  (shellTid,  (char*)&err, sizeof(int));

	// Parse the model
	track = ( ch == 'A' || ch == 'a' ) ? TRACK_A : TRACK_B;
	parse_model( track, &ui->model );

	// Create the timer notifier
	debug ("ui: creating notifier\r\n");
	uiclkTid = Create ( 2, &uiclk_run );
	Send( uiclkTid, (char*)&ch, sizeof(char), 
					(char*)&ch, sizeof(char) );
	debug ("ui: notifier is done\r\n");

	// Start Drawing the ui
	ui_clearScreen( ui->ios2Tid );
	//Getc(iosTid);
	ui_clearScreen( ui->ios2Tid );

	ui_drawMap( ui->ios2Tid, &ui->model );

	// Register with the Name Server
	RegisterAs( UI_NAME );
}

void ui_updateSensor( UI *ui, int idx, int time ) {
	char *name;
	int   i;
	int   len;
	int   k = 0;
	int   t = 0;
	int   val;

	// Package the new entry
	val = (idx << 15) & time;

	// Store the new data in the buffer
	rb_force ( &ui->sensors, &val );

	for ( i = 0; i < NUM_DISP_SENSORS - 1; i ++ ) {
		k = (ui->sensorsBuf[i] >> 15);
		t = ((ui->sensorsBuf[i] << 15) >> 15);
		
		name = ui->model.nodes[k].name;
		len = strlen (name);

		ui_strPrintAt (ui->ios2Tid, 5*i, 20, name, RED_FC, WHITE_BC);
		ui_intPrintAt (ui->ios2Tid, (5*i)+len, 20, "%d", t, RED_FC, WHITE_BC);
	}
}

void ui_displayPrompt( int ios2Tid, char *fmt, char *str ) {
	cprintf(ios2Tid, "\033[36m");
	cprintf(ios2Tid, "\033[40B");
	cprintf(ios2Tid, fmt, str);
	cprintf(ios2Tid, "\033[37m");
}

void ui_updateMap( UI* ui, int idx, int state ) {
	Node *n = &ui->model.nodes[idx];

	switch (n->type) {
		case NODE_SWITCH:
			ui_strPrintAt( ui->ios2Tid, 
						   5 + n->x/8, 
						   n->y/16, 
						   (char*)&state,
						   MAGENTA_FC, BLACK_BC);
			break;
		case NODE_STOP:
		case NODE_SENSOR:
			// Do nothing.
			break;
	}
}

void ui_updateTrainLocation( UI *ui, int idx, int dist) {

}

void ui_displayTime( int ios2Tid, int time ) {
	int tens, mins, secs;
	tens = time % 10;
	secs = (time / 10) % 60;
	mins = time / 600;
	
	ui_intPrintAt (ios2Tid, 65, 7, "%02d:", mins, GREEN_FC, BLACK_BC);
	ui_intPrintAt (ios2Tid, 68, 7, "%02d:", secs, CYAN_FC, BLACK_BC);
	ui_intPrintAt (ios2Tid, 71, 7, "%01d", tens, RED_FC, BLACK_BC);
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
"					 _    _", 			
"					/ \\  / \\",      		
"                   |@|  |@|",             
"				   _\\_/__\\_/_",          	
"				  /          \\",                  
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
"             \\/  \\/  \\/  \\/  \\/  \\/             	" };

	int i;
	for ( i = 0; i < 20; i ++ ) {
		ui_strPrintAt( ios2Tid, 1, i+5,
				    (char*)frog[i], GREEN_FC, BLACK_BC );
	}

}

void ui_drawMap (int ios2Tid, TrackModel *model) {
	
	cprintf (ios2Tid, "\033(0");
	
	cprintf (ios2Tid, "lkmjqxn~utvw");

	int  i;
	char q[] = "q";

	for ( i = 0; i < 80; i ++ ) {
		int j;
		for ( j = 0; j < 20; j ++ ) {
			ui_strPrintAt (ios2Tid, i, j, q, BLACK_FC, GREEN_BC);
		}
	}
	cprintf (ios2Tid, "\033(B");
	
	for ( i = 0; i < model->num_nodes; i ++ ) {
		char ch[2];
		ch[1] = '\0';

		switch (model->nodes[i].type) {
			case NODE_STOP:
				*ch = 'e';
				break;
			case NODE_SENSOR:
				*ch = 'a';
				break;
			case NODE_SWITCH:
				*ch = 's';
				break;
		}
		
		ui_strPrintAt (ios2Tid, 
					5 + model->nodes[i].x/8, 
					2 + model->nodes[i].y/17, 
					ch,
					BLUE_FC, RED_BC);
	}

	cprintf (ios2Tid, "\033(B");
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
