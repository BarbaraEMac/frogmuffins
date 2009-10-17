/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __SYSCALLS_H__
#define __SYSCALLS_H__

#include "globals.h"
#include "td.h"

typedef enum messageType {
	SEND_2_RCV =1,
	REPLY_2_SEND
} MsgType;


int send (TD *sender, TDM *mgr, TID tid);

int receive (TD *receiver, TID *tid);

int reply (TD *sender, TDM *mgr, TID tid, char *reply, int rpllen);

int passMessage (TD *sender, TD *receiver, MsgType reply);

void awaitEvent (TD *td, TDM *mgr, enum INTERRUPTS type);

#endif
