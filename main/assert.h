/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#ifndef __ASSERT_H__
#define __ASSERT_H__

#ifdef DEBUG 

void __assert ( int exp, char *mess );

#define assert(exp) __assert(exp, #exp);

#else

#define assert(exp) do {} while (0);

#endif

#endif
