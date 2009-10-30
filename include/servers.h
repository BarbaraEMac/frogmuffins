/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __SERVERS_H__
#define __SERVERS_H__

#define CLOCK_NAME 		"ClockServer"
#define SERIALIO_NAME  "IOServer"

#define NUM_ENTRIES		256
#define ENTRY_LEN		80
#define NUM_NS_ENTRIES	256

#include "globals.h"
#include "requests.h"

enum SvrRequest {
	REGISTERAS = 1,
	WHOIS,
	DELAY,
	TIME,
	DELAYUNTIL,
	NOTIFY,
	GETC,
	PUTC,
	GETSTR,
	PUTSTR,
	NOTIFY_CH,
	NOTIFY_CTS //12
};

// Any request made to the NameServer needs to be of this form.
typedef struct {
	enum SvrRequest type;
	TaskName		name;
} NSRequest;

// Any request made to the ClockServer needs to be of this form.
typedef struct {
	enum SvrRequest type;
	int  			ticks;
} CSRequest;

// Any request made to the SerialIO Server has this form.
typedef struct {
	enum SvrRequest type;
	int				channel;
	char 		    data[ENTRY_LEN];
	int				len;
} IORequest;

//-----------------------------------------------------------------------------
//--------------------------------Name Server----------------------------------
//-----------------------------------------------------------------------------
/**
 * The main function for the Name Server task.
 */
void ns_run ();

//-----------------------------------------------------------------------------
//-------------------------------Clock Server----------------------------------
//-----------------------------------------------------------------------------

/**
 * All of the Clock Server work is done in here.
 */
void cs_run ();

//-----------------------------------------------------------------------------
//--------------------------------Serial IO------------------------------------
//-----------------------------------------------------------------------------
/**
 * All of the Serial IO work is done in here.
 */
void ios1_run ();

void ios2_run ();

#endif
