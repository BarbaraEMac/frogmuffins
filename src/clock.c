/*
 * CS 452: Clock Stuff
 *
 */

#include <ts7200.h>
/*
 * Some simple timer control
 *
 */
int* clock_init( int clock_base, int enable, int val ) {
    int *clock_ldr = (int *)( clock_base + LDR_OFFSET );
    int *clock_ctl = (int *)( clock_base + CRTL_OFFSET );
    // set the loader value, first disabling the timer
    *clock_ctl = 0;
    *clock_ldr = val;
    // set the enable bit
    if( enable ) { *clock_ctl = ENABLE_MASK; }
    // return a pointer to the clock
    return (int *)( clock_base + VAL_OFFSET );
}

void clock_stop( int clock_base ) {
    clock_init( clock_base, 0, 0);
}

void wait( int ms ) {
    int * clock = clock_init( TIMER1_BASE, 1, ms*2 );   // turn on the clock
    while( *clock > 0 ) {}                              // wait for the clock to finish 
    clock_stop( TIMER1_BASE );                          // turn off the clock
}
