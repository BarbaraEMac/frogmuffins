/*
 * bwio.c - busy-wait I/O routines for diagnosis
 *
 * Specific to the TS-7200 ARM evaluation board
 *
 */

#include <bwio.h>
#include <ts7200.h>

#include "string.h"


int bwsendc( int channel, char c, int timeout ) {
	volatile int *time;
	UART *uart;
	switch( channel ) {
	case COM1:
		uart = UART1;
		break;
	case COM2:
		uart = UART2;
		break;
	default:
		return -1;
		break;
	}

    time = clock_init( TIMER2, 1, 0, timeout );
    // wait for the input to be ready
    while( (( uart->flag & TXFF_MASK ) || !( uart->flag & CTS_MASK ))
            && ( *time > 0 ) ) {}
    clock_stop( TIMER2 ); 
    if( ( uart->flag & TXFF_MASK ) || !( uart->flag & CTS_MASK ) ) {
        return -1;
    }
	uart->data = c;
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
    volatile int *time;
	UART *uart;

    switch( channel ) {
    case COM1:
		uart = UART1;
        break;
    case COM2:
		uart = UART2;
        break;
    default:
        return 0;
        break;
    }
    if( timeout ) {
        time = clock_init( TIMER2, 1, 0, timeout );
        while( !( uart->flag & RXFF_MASK ) && (*time > 0) ) {}  //busy-wait
        clock_stop( TIMER2 );
    }
    if( (uart->flag & RXFF_MASK) ) {
        *c = uart->data;
        return 1;
    }
    return 0;
}

void bwformat ( int channel, const char *fmt, va_list va ) {
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
			// See if the width is set
            if( ch >= '1' && ch <= '9' ) {
				fmt--;		// we already read one of the digits
				w = atoi( &fmt, 10 );
				ch = *(fmt++);
			}
			switch( ch ) {
			case 0: return;
			case 'c':
				bwputc( channel, va_arg( va, char ) );
				break;
			case 's':
				bwputw( channel, w, ' ', va_arg( va, char* ) );
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

void bwprintf( int channel, const char *fmt, ... ) {
        va_list va;

        va_start(va,fmt);
        bwformat( channel, fmt, va );
        va_end(va);
}
