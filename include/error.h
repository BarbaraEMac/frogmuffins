/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __ERROR_H__
#define __ERROR_H__

enum ERROR {
	
	NO_ERROR = 0,			// Everything ran smoothly!
	//-10000
	NEG_TID = -10000,		// The task id is negative
	DEFUNCT_TID,			// The corresponding td is defunct
	OLD_TID, 				// Not from the current generation
	INVALID_TID,			// The task id is not valid
	OUT_OF_BOUNDS,			// The address is not within the task's address space
	// -9995
	RCV_BUFFER_FULL,		// The Receive() buffer is full
	RPLY_BUFFER_FULL,		// The Reply() buffer is full
	NULL_ADDR,				// A buffer address is 0 (null)
	SNDR_NOT_RPLY_BLKD,		// The Sender is in a bad state
	INVALID_PRIORITY,		// The given priority is invalid
	// -9990
	NO_TDS_LEFT,			// No more useable TDs - Cannot "create" more
	INVALID_EVENTID,		// The event id is not valid
	NO_DRIVER,				// No driver is installed
	NOT_FOUND,				// Name Server could not locate the corresponding task
	NS_INVALID_REQ_TYPE,	// Invalid Name Server request type
	// -9985
	CS_INVALID_REQ_TYPE,	// Invalid Clock Server request type
	IOS_INVALID_REQ_TYPE,	// Invalid Serial IO request type
	TS_INVALID_REQ_TYPE,	// Invalid Track Server request type
	INVALID_TRAIN,			// Invalid train number sent to train controller
	INVALID_DIR,			// Invalid direction for a train
	// -9980
	INVALID_SWITCH,			// Invalid switch number
	CONNECTION_TIMEOUT,		// Connection to the train controller failed
	CANNOT_INIT_SWITCHES,	// Switch initialization failed
	SERIAL_OVERRUN,			// Serial IO data was overrun
	INVALID_UART_SPEED,		// The uart speed specified is not valid
	// -9975
	UNHANDLED_UART_INTR,	// The interrupt intercepted is not handled
	INVALID_UART_ADDR,		// The addres passed in is not a UART
	TIMEOUT,				// A timeout has occured
	DET_INVALID_REQ_TYPE,	// Invalid Track Detective request type
	DET_NO_MATCH,			// No match was found
	// --9970
	DET_RETRACTED,			// The request was retracted
	INVALID_NODE_NAME,		// Invalid node name
	INVALID_NODE_IDX,		// Invalid node index
	RP_INVALID_REQ_TYPE,	// Invalid request type for the Route Planner
	INVALID_SENSOR_IDX,		// Invalid sensor index
	// --9965
	INVALID_TRAIN_SPEED,	// Invalid train speed (avg speed)
	INVALID_TRACK,			// Invalid track identifier
	NO_PATH					// There is no path for the train
} error;

#endif
