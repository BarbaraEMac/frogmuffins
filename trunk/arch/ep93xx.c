/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include <ts7200.h>
#include <math.h>
#include "error.h"

inline int readMemory(int addr) {
	int volatile *mem = (int *) (addr);
	return *mem;
}

inline void writeMemory(int addr, int value) {
	int volatile *mem = (int *) (addr);
	*mem = value;
}

void intr_set( Interrupt eventId, int state) {
	VIC * vic;
	int mask;
	// Figure out which VIC we're using and set the mask appropriately
	if( eventId < 32 ) {
		vic = VIC1;
		mask = 1 << eventId;
	} else {
		vic = VIC2;
		mask = 1 << (eventId - 32);
	}
	// turn the interrupt on or off depending on the case
	if( state ) {
		vic->intEnable |= mask;
	} else {
		vic->intEnClear |= mask;
	}
}

Interrupt intr_get() {
	VIC *vic1 = VIC1, *vic2 = VIC2;
	if( vic1->IRQStatus ) {
		return ctz( vic1->IRQStatus );
	} else {
		return ctz( vic2->IRQStatus ) + 32;
	}
}

void intr_allOff() {
	VIC *vic1 = VIC1, *vic2 = VIC2;
	vic1->intEnClear = 0xFFFFFFFF;
	vic2->intEnClear = 0xFFFFFFFF;
}

/*
 * The UARTs are initialized by RedBoot to the following state
 * 	115,200 bps
 * 	8 bits
 * 	no parity
 * 	fifos enabled
 */

int uart_setFifo( UART *uart, int state ) {
	int buf;
	buf = uart->lcrh;
	buf = state ? buf | FEN_MASK : buf & ~FEN_MASK;
	uart->lcrh = buf;
	return 0;
}

int uart_setSpeed( UART *uart, int speed ) {
	switch( speed ) {
	case 115200:
		uart->lcrm = 0x0;
		uart->lcrl = 0x3;
		return 0;
	case 2400:
		uart->lcrm = 0x0;
		uart->lcrl = 0xbf;
		return 0;
	default:
		return INVALID_UART_SPEED;
	}
}

void cache_on() {
	asm("mov r0, #0");
	// Invalidate the cache before using it
	asm("mcr p15, 0, r0, c7, c5, 0");
	// Read the current state of the cache
	asm("mrc p15, 0, r0, c1, c0, 0");
	// Enable instruction cache
	asm("orr r0, r0, #(0x1 <<12)");
	// Enable data cache
	//asm("orr r0, r0, #(0x1 <<2)");
	// Save the changes back to the cache control register
	asm("mcr p15, 0, r0, c1, c0, 0");
}


void cache_off() {
	// Read the current state of the cache
	asm("mrc p15, 0, r0, c1, c0, 0");
	// Disable instruction cache
	asm("bic r0, r0, #(0x1 <<12)");
	// Disable data cache
	//asm("bic r0, r0, #(0x1 <<2)");
	// Save the changes back to the cache control register
	asm("mcr p15, 0, r0, c1, c0, 0");
}


int volatile *clock_init( Clock *clock, int enable, int interrupt, int val ) {
    // Set the loader value, first disabling the timer
    clock->ctl = 0;
    clock->ldr = val;
    // Set the enable bit and interrupt
    if( enable ) { clock->ctl = ENABLE_MASK; }
	if( interrupt ) { clock->ctl |= MODE_MASK; }
    // Return a pointer to the time
    return &(clock->val);
}

void clock_stop( Clock *clock ) {
    clock_init( clock, 0, 0, 0 );
}

void clock_bwwait( int ms ) {
	// Start the timer
    int volatile* time = clock_init( TIMER1, 1, 0, ms*2 );
	// Wait the required time
    while( *time > 0 ) {}
	// Stop the timer
    clock_stop( TIMER1 );
}
