/**
 * CS 451: kernel
 * becmacdo
 * dgoc
 */

#ifndef __CLOCK_H__
#define __CLOCK_H__

/**
 * Initialize a hardware timer.
 * clock_base - The timer to user (TIMER1_BASE, TIMER2_BASE, or TIMER3_BASE)
 * enable - 1 if the timer should be enabled. 0, otherwise.
 * interrupt - 1 if interrupts should be turned on. 0, otherwise.
 * val - The initial value for the timer.
 */
int* clock_init( int clock_base, int enable, int interrupt, int val );

/**
 * Stop a timer from counting.
 * clock_base - The timer to user (TIMER1_BASE, TIMER2_BASE, or TIMER3_BASE)
 */
void clock_stop( int clock_base );

/**
 * We probably shouldn't use this ....
 */
void wait( int ms );

#endif
