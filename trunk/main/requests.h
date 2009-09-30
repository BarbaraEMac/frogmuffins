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
	enum RequestCode type; // the type of the request (including number of arguments)
	int const * const args;	// place in user memory where arguments are stored
	// NOTE: The 5th argument is in args[22] 
	// and NOT args[4] as would be expected
	

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

int Test5( int a0, int a1, int a2, int a3, int a5) ;

#endif