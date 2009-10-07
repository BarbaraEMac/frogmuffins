/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __SYSCALLS_H__
#define __SYSCALLS_H__

#include "td.h"

int send (TD *sender, const PQ *pq, TID tid);

int receive (TD *receiver, TID *tid);

int reply (TD *sender, PQ *pq, TID tid, char *reply, int rpllen);

int passMessage (const TD *sender, const TD *receiver, int *copyLen, int reply);

#endif
