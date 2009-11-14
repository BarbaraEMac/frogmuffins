/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#define DEBUG 2
#include <string.h>

#include "debug.h"
#include "requests.h"
#include "servers.h"
#include "switch.h"

#define SWI(n) asm volatile("swi #" #n)

typedef struct {
	TID tid;
	union {
		int result;
		int ticks;
	};
} helperMsg;
	

int Create (int priority, Task code ) {
	SWI(1);
	//return syscall(priority, (int) code, 0, CREATE);
}

TID MyTid () {
	SWI(2);
//	return syscall(0, 0, 0, MYTID);
}

TID MyParentTid() {
	SWI(3);
	//return syscall(0, 0, 0, MYPARENTTID);
}

void Pass () {
	SWI(4);
	//syscall(0, 0, 0, PASS);
}

void Exit () {
	SWI(5);
	//syscall(0, 0, 0, EXIT);
}

// This is what a task calls.
int Send (TID tid, char *msg, size_t msglen, char *reply, size_t rpllen) {
	SWI(6);
}


int Receive (TID *tid, char *msg, size_t msglen) {
	SWI(7);
}


int Reply (TID tid, char *reply, size_t rpllen) {
	SWI(8);
}

int RegisterAs (char *name) {
	NSRequest req;
	int ret;
	req.type = REGISTERAS;
	strncpy( req.name, name, sizeof(req.name));
	Send( NS_TID, (char*) &req, sizeof(NSRequest), (char*) &ret, sizeof(int) );
	return ret;
}


TID WhoIs (char *name) {
	NSRequest req;
	TID tid;
	req.type = WHOIS;
	strncpy(req.name, name, sizeof(req.name));
	Send( NS_TID, (char*) &req, sizeof(NSRequest), (char*) &tid, sizeof(TID) );
	return tid;
}

int AwaitEvent (int eventid, char *event, size_t eventlen) {
	SWI(9);
}

int InstallDriver (int eventid, Driver driver) {
	SWI(10);
}

int Destroy (TID tid) {
	SWI(11);
}

int Delay (int ticks, TID csTid) {
	int		  err;
	int		  currentTime;
	CSRequest req;
	
	req.type  = DELAY;
	req.ticks = ticks;
	
	err = Send(csTid, (char*) &req, sizeof(CSRequest), (char*) &currentTime, 			   sizeof(int));
	return err;
}

int Time (TID csTid) {
	int time;
	CSRequest req;
	req.type  = TIME;
	req.ticks = 0;
	Send(csTid, (char*)&req, sizeof(CSRequest), (char*)&time, sizeof(int));

	return time;
}

int DelayUntil (int ticks, TID csTid) {
	int		  err;
	int 	  currentTime;
	CSRequest req;
	
	req.type  = DELAYUNTIL;
	req.ticks = ticks;
	
	err = Send(csTid, (char*) &req, sizeof(CSRequest), (char*) &currentTime,
		 	   sizeof(int));
	return err;
}

int Getc (TID iosTid) {
	char      ret;
	IORequest req;
	
	req.type    = GETC;
	req.len 	= 0;
	Send(iosTid, (char*)&req, IO_REQUEST_SIZE, &ret, sizeof(char));
	
	return ret;
}

int Putc (char ch, TID iosTid) {
	IORequest req;
	
	req.type    = PUTC;
	req.data[0] = ch;
	req.len     = 1;

	return Send(iosTid, (char*)&req, IO_REQUEST_SIZE + 1, 0, 0);
}

int PutStr (const char *str, int strLen, TID iosTid) {
	assert ( strLen <= 80 );
	
	IORequest req;
	
	req.type    = PUTSTR;
	memcpy (req.data, str, strLen);
	req.len     = strLen;

	return Send(iosTid, (char*)&req, IO_REQUEST_SIZE + strLen, 0, 0);
	
}

void Purge (TID iosTid) {
	IORequest req;

	req.type    = PURGE;
	req.len 	= 0;
	Send(iosTid, (char*)&req, IO_REQUEST_SIZE, 0, 0);
}

void getcHelper() {
	helperMsg 	msg;
	TID			parent;
	// Get instructions on what to do
	Receive( &parent, (char *) &msg, sizeof(helperMsg) );
	Reply( parent, 0 , 0 );
	// Get a character from the IO server
	msg.result = Getc( msg.tid );
	// Return result back to parent
	Send( parent, (char*) &msg, sizeof(helperMsg), 0, 0);
}

void delayHelper() {
	helperMsg 	msg;
	TID			parent;
	// Get instructions on what to do
	Receive( &parent, (char *) &msg, sizeof(helperMsg) );
	Reply( parent, 0 , 0 );
	// Wait for specified number of ticks
	msg.result = Delay( msg.ticks, msg.tid );
	// Return result back to parent
	Send( parent, (char*) &msg, sizeof(helperMsg), 0, 0);
}

	
int TimeoutGetc( TID iosTid, TID csTid, int timeout ) {
	helperMsg 	msg;
	TID 		getcTid = Create(3, &getcHelper );
	TID 		delayTid = Create(3, &delayHelper );
	TID			tid;
	// Start off the getc helper
	msg.tid = iosTid;
	Send( getcTid, (char *) &msg, sizeof(helperMsg), 0, 0 );
	// Start off the delay helper
	msg.tid = csTid;
	msg.ticks = timeout;
	Send( delayTid, (char *) &msg, sizeof(helperMsg), 0, 0 );
	// Wait for one of them to finish
	Receive( &tid, (char *) &msg, sizeof(helperMsg) );
	// Destroy them
	Destroy( getcTid );
	Destroy( delayTid );
	if( msg.tid == delayTid ) return TIMEOUT;
	else return msg.result;
}
/*

void WaitFor ( int n ) {
	debug("WaitFor: tid:%d, n:%d\r\n", MyTid(), n );
	TID 	tid;
	char	msg;
	while( n-- ) {
		Receive( &tid, &msg, sizeof(msg) );
		Reply( tid, &msg, sizeof(msg) );
	}
}

int Sync ( TID tid ) {
	debug("Sync: tid:%d, with:%d\r\n", MyTid(), tid );
	char msg, rpl;
	return Send( tid, &msg, sizeof(msg), &rpl, sizeof(rpl) );
}*/
