/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include <ts7200.h>


inline int readMemory(int addr) {
	int volatile *mem = (int *) (addr);
	return *mem;
}

inline void writeMemory(int addr, int value) {
	int volatile *mem = (int *) (addr);
	*mem = value;
}

void intr_on( Interrupt eventId ) {
	// Turn off interrupt in the proper VIC
	if( eventId < 32 ) {
		writeMemory( VIC1_BASE + VIC_INT_ENABLE, (1 << eventId ) );
	} else {
		writeMemory( VIC2_BASE + VIC_INT_ENABLE, (1 << (eventId - 32)) );
	}
}

void intr_off( Interrupt eventId ) {
	// Turn off interrupt in the proper VIC
	if( eventId < 32 ) {
		writeMemory( VIC1_BASE + VIC_INT_EN_CLR, (1 << eventId ) );
	} else {
		writeMemory( VIC2_BASE + VIC_INT_EN_CLR, (1 << (eventId - 32)) );
	}
}

void intr_allOff() {
	writeMemory( VIC1_BASE + VIC_INT_EN_CLR, 0xFFFFFFFF );
	writeMemory( VIC2_BASE + VIC_INT_EN_CLR, 0xFFFFFFFF );
}

/*
 * The UARTs are initialized by RedBoot to the following state
 * 	115,200 bps
 * 	8 bits
 * 	no parity
 * 	fifos enabled
 */

int uart_setfifo( UART *uart, int state ) {
	int buf;
	buf = uart->lcrh;
	buf = state ? buf | FEN_MASK : buf & ~FEN_MASK;
	uart->lcrh = buf;
	return 0;
}

int uart_setspeed( UART *uart, int speed ) {
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
		return -1;
	}
}

void cache_on() {
	// TODO
	// MRC p15, 0, Rd, c0, c0, 1 ; returns Cache Type register
	asm("mrc	p15, 0, r0, c0, c0, 1");
	// Enable instruction cache
	// Enable data cache
}


void cache_off() {
	// TODO
}


int volatile *clock_init( Clock *clock, int enable, int interrupt, int val ) {
    // set the loader value, first disabling the timer
    clock->ctl = 0;
    clock->ldr = val;
    // set the enable bit
    if( enable ) { clock->ctl = ENABLE_MASK; }
	if( interrupt ) { clock->ctl |= MODE_MASK; }
    // return a pointer to the clock
    return &(clock->val);
}

void clock_stop( Clock *clock ) {
    clock_init( clock, 0, 0, 0 );
}

void clock_bwwait( int ms ) {
    int volatile* time = clock_init( TIMER1, 1, 0, ms*2 );// turn on the clock
    while( *time > 0 ) {}				// wait for the clock to finish 
    clock_stop( TIMER1 );				// turn off the clock
}
