/*
 * bwio.c - busy-wait I/O routines for diagnosis
 *
 * Specific to the TS-7200 ARM evaluation board
 *
 */

#include <ts7200.h>
#include <bwio.h>

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

    clock = clock_init( TIMER2_BASE, 1, timeout );
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

char c2x( char ch ) {
	if ( (ch <= 9) ) return '0' + ch;
	return 'a' + ch - 10;
}

int bwputx( int channel, char c ) {
	char chh, chl;

	chh = c2x( c / 16 );
	chl = c2x( c % 16 );
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
        clock = clock_init( TIMER2_BASE, 1, timeout );
        while( !( *flags & RXFF_MASK ) && (*clock > 0) ) {}          //busy-wait
        clock_stop( TIMER2_BASE );
    }
    if( (*flags & RXFF_MASK) ) {
        *c = *data;
        return 1;
    }
    return 0;
}


int bwa2d( char ch ) {
	if( ch >= '0' && ch <= '9' ) return ch - '0';
	if( ch >= 'a' && ch <= 'f' ) return ch - 'a' + 10;
	if( ch >= 'A' && ch <= 'F' ) return ch - 'A' + 10;
	return -1;
}

char bwa2i( char ch, char **src, int base, int *nump ) {
	int num, digit, sign;
	char *p;

	p = *src; num = 0; sign = 1;
    if( ch == '-' ) {
        sign = -1;
        ch = *p++;
    }
	while( ( digit = bwa2d( ch ) ) >= 0 ) {
		if ( digit > base ) break;
		num = num*base + digit;
		ch = *p++;
	}
	*src = p; *nump = num * sign;
	return ch;
}

void bwui2a( unsigned int num, unsigned int base, char *bf ) {
	int n = 0;
	int dgt;
	unsigned int d = 1;
	
	while( (num / d) >= base ) d *= base;
	while( d != 0 ) {
		dgt = num / d;
		num %= d;
		d /= base;
		if( n || dgt > 0 || d == 0 ) {
			*bf++ = dgt + ( dgt < 10 ? '0' : 'a' - 10 );
			++n;
		}
	}
	*bf = 0;
}

void bwi2a( int num, char *bf ) {
	if( num < 0 ) {
		num = -num;
		*bf++ = '-';
	}
	bwui2a( num, 10, bf );
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
				ch = bwa2i( ch, &fmt, 10, &w );
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
				bwui2a( va_arg( va, unsigned int ), 10, bf );
				bwputw( channel, w, lz, bf );
				break;
			case 'd':
				bwi2a( va_arg( va, int ), bf );
				bwputw( channel, w, lz, bf );
				break;
			case 'x':
				bwui2a( va_arg( va, unsigned int ), 16, bf );
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


int scan ( char *src, char *fmt, va_list va ) {
	char ch, ig, rd, *dest;
	int w, *val;
    unsigned int *uval;

    rd = *(src++);	
	while ( ( ch = *(fmt++) ) && rd ) {
//        bwprintf( COM2, "\n\rch=%c, rd=%c", ch, rd);
		if ( ch != '%' ) {
            if( ch != rd )	return -1;
            rd = *(src++);
        } else {
			ig =0; w = 0;
			ch = *(fmt++);
			if ( ch == '*' ) {
				ig = 1; ch = *(fmt++);
            }
			switch ( ch ) {
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				ch = bwa2i( ch, &fmt, 10, &w );
				break;
			}
            //bwprintf( COM2, "\n\r2:ch=%c, rd=%c", ch, rd);
			switch( ch ) {
			case 'c':
				*(va_arg( va, char* ) ) = rd;
                rd = *(src++);
				break;
			case 's':
                dest = va_arg( va, char* );
                if( rd <= 32 || rd >= 127 ) return -5;
                while( rd > 32 && rd < 127 ) { // stop at whitespace
                    *dest++ = rd;
                    rd = *src++;
                }
				break;
			case 'u':
                uval =	va_arg( va, unsigned int *);
                rd = bwa2i(rd, &src, 10, uval );
				break;
			case 'd':
				val= va_arg( va, int *);
                rd = bwa2i(rd, &src, 10, val );
				break;
			case 'x':
				val= va_arg( va, int *);
                rd = bwa2i(rd, &src, 16, val );
				break;
			case '%':
				if( rd != ch ) return -3;
                rd = *(src++);
				break;
			}

            //bwprintf( COM2, "\n\r3:ch=%c, rd=%c, val=%d, uval=%u", ch, rd, *val, uval);
		}
	}
    if( !rd && ch ) return -2; // we ran out of input
    return 0;
}
int sscanf( char *src, char *fmt, ... ) {
        int res;
        va_list va;
        
        va_start(va,fmt);
        res = scan( src, fmt, va );
        if( res != 0 ) return res;
        va_end(va);

        return 0;
}
