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

void interruptOn( Interrupt eventId ) {
	// Turn off interrupt in the proper VIC
	if( eventId < 32 ) {
		writeMemory( VIC1_BASE + VIC_INT_ENABLE, (1 << eventId ) );
	} else {
		writeMemory( VIC2_BASE + VIC_INT_ENABLE, (1 << (eventId - 32)) );
	}
}

void interruptOff( Interrupt eventId ) {
	// Turn off interrupt in the proper VIC
	if( eventId < 32 ) {
		writeMemory( VIC1_BASE + VIC_INT_EN_CLR, (1 << eventId ) );
	} else {
		writeMemory( VIC2_BASE + VIC_INT_EN_CLR, (1 << (eventId - 32)) );
	}
}

void interruptAllOff() {
	writeMemory( VIC1_BASE + VIC_INT_EN_CLR, 0xFFFFFFFF );
	writeMemory( VIC2_BASE + VIC_INT_EN_CLR, 0xFFFFFFFF );
}
