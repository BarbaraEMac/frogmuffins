/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#ifndef __ASSERT_H__
#define __ASSERT_H__

#ifdef DEBUG 

void __assert ( int test, char *exp, int line, char *file );

#define assert(exp) __assert(exp, #exp, __LINE__, __FILE__ )

#define debug(args...) bwprintf( COM2, args )

#else

#define assert(exp) do {} while (0)

#define debug(args...) do {} while(0)

#endif

#endif
