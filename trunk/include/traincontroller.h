/**
 * CS 451: Train Controller User Task
 * becmacdo
 * dgoc
 */

#ifndef __TRAIN_CONTROLLER_H__
#define __TRAIN_CONTROLLER_H__

#define TRAIN_CONTROLLER_NAME "TrainController"
#define NUM_TRNS    81
#define NUM_SWTS    256
#define SW_CNT      22
#define SW_WAIT     100
#define SNSR_WAIT   100
#define TRAIN_WAIT  500
#define TRAIN_TRIES 5

#include "requests.h"

enum TCRequestCode {
	RV = 1,
	ST,
	SW,
	TR,
	WH
};

typedef struct {
	enum TCRequestCode type;
	int arg1;
	int arg2;
} TCRequest;


/**
 * A Train Controller
 */
typedef struct {
    char lstSensorCh;
    int  lstSensorNum;
    char speeds   [NUM_TRNS];
    char switches [NUM_SWTS];
} TrainController;

/**
 * Initialize the traincontroller
 */
int tc_init (TrainController *tc);

/**
 * The main function for the Train Controller task.
 */
void tc_run ();

int checkTrain( int train );

int checkDir( int *dir );

int trainSend( char byte1, char byte2, TID iosTid, TID csTid );

int switchSend( char sw, char dir, char* switches, TID iosTid );

int switches_init( char dir, char* switches, TID iosTid );

void pollSensors( TrainController *tc, TID iosTid );
#endif
