/*
 * bwio.c - busy-wait I/O routines for diagnosis
 *
 * Specific to the TS-7200 ARM evaluation board
 *
 */

#include <bwio.h>
#include <clock.h>
#include <ts7200.h>

#include "string.h"

/*
 * The UARTs are initialized by RedBoot to the following state
 * 	115,200 bps
 * 	8 bits
 * 	no parity
 * 	fifos enabled
 */
int bwsetfifo( int channel, int state ) {
	int *line, buf;
	switch( channel ) {
	case COM1:
		line = (int *)( UART1_BASE + UART_LCRH_OFFSET );
	        break;
	case COM2:
	        line = (int *)( UART2_BASE + UART_LCRH_OFFSET );
	        break;
	default:
	        return -1;
	        break;
	}
	buf = *line;
	buf = state ? buf | FEN_MASK : buf & ~FEN_MASK;
	*line = buf;
	return 0;
}

int bwsetspeed( int channel, int speed ) {
	int *high, *low;
	switch( channel ) {
	case COM1:
		high = (int *)( UART1_BASE + UART_LCRM_OFFSET );
		low = (int *)( UART1_BASE + UART_LCRL_OFFSET );
	        break;
	case COM2:
		high = (int *)( UART2_BASE + UART_LCRM_OFFSET );
		low = (int *)( UART2_BASE + UART_LCRL_OFFSET );
	        break;
	default:
	        return -1;
	        break;
	}
	switch( speed ) {
	case 115200:
		*high = 0x0;
		*low = 0x3;
		return 0;
	case 2400:
		*high = 0x0;
		*low = 0xbf;
		return 0;
	default:
		return -1;
	}
}

int bwsendc( int channel, char c, int timeout ) {
	int *flags, *data, *clock;
	switch( channel ) {
	case COM1:
		flags = (int *)( UART1_BASE + UART_FLAG_OFFSET );
		data = (int *)( UART1_BASE + UART_DATA_OFFSET );
		break;
	case COM2:
		flags = (int *)( UART2_BASE + UART_FLAG_OFFSET );
		data = (int *)( UART2_BASE + UART_DATA_OFFSET );
		break;
	default:
		return -1;
		break;
	}

    clock = clock_init( TIMER2_BASE, 1, 0, timeout );
    // wait for the input to be ready
    while( (( *flags & TXFF_MASK ) || !( *flags & CTS_MASK ))
            && ( *clock > 0 ) ) {}
    clock_stop( TIMER2_BASE ); 
    if( ( *flags & TXFF_MASK ) || !( *flags & CTS_MASK ) ) {
//        bwputstr( COM2, "Sending: Connection timeout.\n\r" );
        return -1;
    }
	*data = c;
    return 0;
}

int bwputc( int channel, char c ) {
	int *flags, *data;
	switch( channel ) {
	case COM1:
		flags = (int *)( UART1_BASE + UART_FLAG_OFFSET );
		data = (int *)( UART1_BASE + UART_DATA_OFFSET );
		break;
	case COM2:
		flags = (int *)( UART2_BASE + UART_FLAG_OFFSET );
		data = (int *)( UART2_BASE + UART_DATA_OFFSET );
		break;
	default:
		return -1;
		break;
	}
	while( ( *flags & TXFF_MASK ) ) ;
	*data = c;
	return 0;
}

int bwputx( int channel, char c ) {
	char chh, chl;

	chh = ctox( c / 16 );
	chl = ctox( c % 16 );
	bwputc( channel, chh );
	return bwputc( channel, chl );
}

int bwputr( int channel, unsigned int reg ) {
	int byte;
	char *ch = (char *) &reg;

	for( byte = 3; byte >= 0; byte-- ) bwputx( channel, ch[byte] );
	return bwputc( channel, ' ' );
}

int bwputstr( int channel, char *str ) {
	while( *str ) {
		if( bwputc( channel, *str ) < 0 ) return -1;
		str++;
	}
	return 0;
}

void bwputw( int channel, int n, char fc, char *bf ) {
	char ch;
	char *p = bf;

	while( *p++ && n > 0 ) n--;
	while( n-- > 0 ) bwputc( channel, fc );
	while( ( ch = *bf++ ) ) bwputc( channel, ch );
}

void bwclear( int channel ) {
	int *flags, *data;
	unsigned char c;

	switch( channel ) {
	case COM1:
		flags = (int *)( UART1_BASE + UART_FLAG_OFFSET );
		data = (int *)( UART1_BASE + UART_DATA_OFFSET );
		break;
	case COM2:
		flags = (int *)( UART2_BASE + UART_FLAG_OFFSET );
		data = (int *)( UART2_BASE + UART_DATA_OFFSET );
		break;
	default:
		return;
		break;
	}
	while ( *flags & RXFF_MASK ) { c = *data; }
}

int bwgetc( int channel ) {
	int *flags, *data;
	unsigned char c;

	switch( channel ) {
	case COM1:
		flags = (int *)( UART1_BASE + UART_FLAG_OFFSET );
		data = (int *)( UART1_BASE + UART_DATA_OFFSET );
     //   *flags |= DSR_MASK;
		break;
	case COM2:
		flags = (int *)( UART2_BASE + UART_FLAG_OFFSET );
		data = (int *)( UART2_BASE + UART_DATA_OFFSET );
		break;
	default:
		return -1;
		break;
	}
	while ( !( *flags & RXFF_MASK ) ) ;
	c = *data;
	return c;
}

// read a character from the COM if one is present and return true
// return false otherwise
int bwreadc( int channel, char *c, int timeout) {
    int *flags, *data, *clock;

    switch( channel ) {
    case COM1:
        flags = (int *)( UART1_BASE + UART_FLAG_OFFSET );
        data = (int *)( UART1_BASE + UART_DATA_OFFSET );
        break;
    case COM2:
        flags = (int *)( UART2_BASE + UART_FLAG_OFFSET );
        data = (int *)( UART2_BASE + UART_DATA_OFFSET );
        break;
    default:
        return 0;
        break;
    }
    if( timeout ) {
        clock = clock_init( TIMER2_BASE, 1, 0, timeout );
        while( !( *flags & RXFF_MASK ) && (*clock > 0) ) {}          //busy-wait
        clock_stop( TIMER2_BASE );
    }
    if( (*flags & RXFF_MASK) ) {
        *c = *data;
        return 1;
    }
    return 0;
}

void bwformat ( int channel, char *fmt, va_list va ) {
	char bf[12];
	char ch, lz;
	int w;
	
	while ( ( ch = *(fmt++) ) ) {
		if ( ch != '%' )
			bwputc( channel, ch );
		else {
			lz = ' '; w = 0;
			ch = *(fmt++);
			if ( ch == '0' ) {
				lz = '0'; ch = *(fmt++);
            }
			switch ( ch ) {
			//case '0':
			//	lz = '0'; ch = *(fmt++);
			//	break;
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				ch = atoi( ch, &fmt, 10, &w );
				break;
			}
			switch( ch ) {
			case 0: return;
			case 'c':
				bwputc( channel, va_arg( va, char ) );
				break;
			case 's':
				bwputw( channel, w, 0, va_arg( va, char* ) );
				break;
			case 'u':
				uitoa( va_arg( va, unsigned int ), 10, bf );
				bwputw( channel, w, lz, bf );
				break;
			case 'd':
				itoa( va_arg( va, int ), bf );
				bwputw( channel, w, lz, bf );
				break;
			case 'x':
				uitoa( va_arg( va, unsigned int ), 16, bf );
				bwputw( channel, w, lz, bf );
				break;
			case '%':
				bwputc( channel, ch );
				break;
			}
		}
	}
}

void bwprintf( int channel, char *fmt, ... ) {
        va_list va;

        va_start(va,fmt);
        bwformat( channel, fmt, va );
        va_end(va);
}
