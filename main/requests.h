/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */


// These are really system calls - Cowan calls them "requests"

// More to be added later
enum REQUEST_CODE {
    CREATE = 0,
    MYTID,
    MYPARENTTID,
    PASS,
    EXIT
} requestCode;

typedef struct request {
    int arg0;   // First argument to function
	int arg1;   // Second argument to function
	int arg2;   // Third argument to function
	enum REQUEST_CODE type; //   
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
