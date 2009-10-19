/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * This is a debug printing and assert file.
 * If DEBUG is not defined in any file, then all assert and
 * debug statements are stripped.
 */

#ifndef __DEBUG_H__
#define __DEBUG_H__

#ifdef DEBUG 

void __assert (int test, const char *exp, int line, const char *file);

/*
 * Assert that the given expression is true.
 * If it isn't, the expression, line number, and file are output.
 */
#define assert(exp) __assert(exp, #exp, __LINE__, __FILE__ )

/**
 * Print the string to standard output.
 */
#define debug(args...) bwputstr( COM2, "\033[32m\t"); bwprintf( COM2, args ); bwputstr( COM2, "\033[37m")

#else

// GCC strips out do while's that do nothing
#define assert(exp)

// GCC strips out do while's that do nothing
#define debug(args...) do {} while(0)

#endif

#define error(code, msg) bwprintf (COM2, "\033[35mERROR: %d %s on line %d in file %s.\r\n\033[37m", code, msg, __LINE__, __FILE__);

#endif
