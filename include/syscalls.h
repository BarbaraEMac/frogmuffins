/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __SYSCALLS_H__
#define __SYSCALLS_H__

int send (int tid, char *msg, int msglen, char *reply, int rpllen);

int receive (int *tid, char *msg, int msglen);

int reply (int tid, char *reply, int rpllen);

int registerAs (char *name);

int whoIs (char *name);

#endif
