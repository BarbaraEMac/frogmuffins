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

#include "globals.h"

/**
 * Start the command prompt shell running.
 */
void shell_run ();

/**
 * Given an input command, error check it and execute it.
 */
void shell_exec ( char *command, TID tcTid );

#endif
