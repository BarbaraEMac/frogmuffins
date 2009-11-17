/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 *
 * A command prompt for the operating system.
 * Fun secret commands will be implemented later.
 */

#ifndef __SHELL_H__
#define __SHELL_H__

/**
 * Start the command prompt shell running.
 */
void bootstrap ();

// Use this function to grab a line of data before the shell starts.
void shell_inputData 	( char *input, bool reset );



#endif
