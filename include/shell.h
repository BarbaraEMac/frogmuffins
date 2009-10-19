/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __SHELL_H__
#define __SHELL_H__

/**
 * Start the command prompt shell running.
 */
void shell_run ();

/**
 * Given an input command, error check it and execute it.
 */
void shell_exec ( char *command );

#endif
