/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __SYSCALLS_H__
#define __SYSCALLS_H__

#include "td.h"

int send (TD *sender, PQ *pq, TID tid);

int receive (TD *receiver, PQ *pq, TID *tid);

void passMessage (TD *sender, TD *receiver);

int reply (TD *sender, PQ *pq, TID tid, char *reply, int rpllen);

int registerAs (char *name);

int whoIs (char *name);

#endif
