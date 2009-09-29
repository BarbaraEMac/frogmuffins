/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __REQUESTS_H__
#define __REQUESTS_H__

// These are really system calls - Cowan calls them "requests"

// More to be added later
enum RequestCode {
    CREATE = 1,
    MYTID,
    MYPARENTTID,
    PASS,
    EXIT
};

typedef struct {
    int arg0;   // First argument to function
	int arg1;   // Second argument to function
	int arg2;   // Third argument to function
	enum RequestCode type; //   
} Request;

/**
 * Create a new task with given priority and start function
 */
int Create (int priority, void (*code) () );

/**
 * Return the task id
 */
int MyTid ();

/**
 * Return the task id of the parent task.
 */
int MyParentTid();

/**
 * Cease execution, but stay ready to run.
 */
void Pass ();

/**
 * Terminate execution forever.
 */
void Exit ();

#endif
