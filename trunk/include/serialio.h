/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __SERIAL_IO_SERVER_H__
#define __SERIAL_IO_SERVER_H__

#define SERIAL_IO_NAME "SerialIoServer"
#define NUM_ENTRIES		32
#define	ENTRY_LEN		80

#include "globals.h"

typedef enum {
	GETC = 1,
	PUTC,
	GETSTR,
	PUTSTR
} IORequestTypes;

typedef struct {
	IORequestTypes  type;
	char 			data[ENTRY_LEN];
} IORequest;

typedef struct {
	char sendBuffer[NUM_ENTRIES][ENTRY_LEN];
	char recvBuffer[NUM_ENTRIES][ENTRY_LEN];
	
} SerialIOServer;

void ios_run ();

void ios_init (SerialIOServer *server);

#endif
