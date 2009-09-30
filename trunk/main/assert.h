/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#ifndef __ASSERT_H__
#define __ASSERT_H__

#ifdef DEBUG 

void __assert ( int test, char *exp, char *line, char *file );

#define assert(exp) __assert(exp, #exp, __LINE__, __FILE__ )

#else

#define assert(exp) do {} while (0)

#endif

#endif
