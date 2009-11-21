/**
 * CS 452: UI Stuff
 * becmacdo
 * dgoc
 *
 */

#ifndef __UI_H__
#define __UI_H__

#define STR_LEN		15
#define UI_NAME		"UI_SERVER"

enum UIRequestType{
	CLOCK = 1,
	TRACK_SERVER,
	SHELL,
	DETECTIVE,
	TRAIN
};

typedef struct {
	enum UIRequestType type;

	union {
		int time;
		int dist;
	};
	int  idx;
	char state;
	int  trainId;
//	char fmt[STR_LEN];
//	char str[STR_LEN];
} UIRequest;

typedef enum {
	BLACK_FC = 30,
	RED_FC,
	GREEN_FC,
	YELLOW_FC,
	BLUE_FC,
	MAGENTA_FC,
	CYAN_FC,
	WHITE_FC,
	DEFAULT_FC = 39
} ForeColour;

typedef enum {
	BLACK_BC = 40,
	RED_BC,
	GREEN_BC,
	YELLOW_BC,
	BLUE_BC,
	MAGENTA_BC,
	CYAN_BC,
	WHITE_BC,
	DEFAULT_BC = 49
} BackColour;

void ui_run();
#endif
