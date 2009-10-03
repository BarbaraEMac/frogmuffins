/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * This is a debug printing and assert file.
 * If DEBUG is not defined in any file, then all assert and
 * debug statements are stripped.
 */

#ifndef __ASSERT_H__
#define __ASSERT_H__

#ifdef DEBUG 

void __assert ( int test, char *exp, int line, char *file );

/*
 * Assert that the given expression is true.
 * If it isn't, the expression, line number, and file are output.
 */
#define assert(exp) __assert(exp, #exp, __LINE__, __FILE__ )

/**
 * Print the string to standard output.
 */
#define debug(args...) bwprintf( COM2, args )

#else

// GCC strips out do while's that do nothing
#define assert(exp) do {} while (0)

// GCC strips out do while's that do nothing
#define debug(args...) do {} while(0)

#endif

#endif
