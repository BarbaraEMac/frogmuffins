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

int uart_setfifo( int channel, int state ) {
	UART *uart;
	int buf;
	switch( channel ) {
		case COM1:
			uart = (UART*) UART1_BASE;
			break;
		case COM2:
			uart = (UART*) UART2_BASE;
			break;
		default:
			return -1;
			break;
	}
	buf = uart->lcrh;
	buf = state ? buf | FEN_MASK : buf & ~FEN_MASK;
	uart->lcrh = buf;
	return 0;
}

int uart_setspeed( int channel, int speed ) {
	UART *uart;
	switch( channel ) {
		case COM1:
			uart = (UART*) UART1_BASE;
			break;
		case COM2:
			uart = (UART*) UART2_BASE;
			break;
		default:
			return -1;
			break;
	}
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

