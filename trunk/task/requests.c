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

#define SWI(n) asm("swi #" #n)

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
	req.len     = 0;

	Send(iosTid, (char*)&req, IO_REQUEST_SIZE, (char*)&ret, sizeof(char));
	
	return ret;
}

int Putc (char ch, TID iosTid) {
	char 	  reply;
	IORequest req;
	
	req.type    = PUTC;
	req.data[0] = ch;
	req.len     = 1;

	return Send(iosTid, (char*)&req, IO_REQUEST_SIZE + 1, &reply, sizeof(char));
}

int PutStr (const char *str, int strLen, TID iosTid) {
	assert ( strLen <= 80 );
	
	char 	  reply;
	IORequest req;
	
	req.type    = PUTSTR;
	memcpy (req.data, str, strLen);
	req.len     = strLen;

	return Send(iosTid, (char*)&req, IO_REQUEST_SIZE + strLen, &reply, sizeof(char));
	
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
