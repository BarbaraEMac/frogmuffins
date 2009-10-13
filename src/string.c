/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include "bwio.h"
#include "string.h"

char * strncpy ( char *destination, const char * source, size_t num ) {
	int i;
	for ( i=0; (i < num) && source[i]; i++ ) {
		destination[i] = source[i];
	}
	for ( ; i < num; i++ ) {
		destination[i] = '\0';
	}
	return destination;
}


char * memcpy ( char * destination, const char * source, size_t num ) {
	while( (--num) >= 0 ) destination[num] = source[num];
	return destination;
}

int strcmp (const char *str1, const char *str2) {
	while ( *str1 == *str2 ) {
		str1 ++;
		str2 ++;

		if ( (*str1 == 0) && (*str2 == 0) ) {
			return 0;
		}
		if ( (*str1 == 0) || (*str2 == 0) ) {
			break;
		}
	}
	
	return (*str1 < *str2) ? -1 : 1;
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
				ch = atoi( ch, &fmt, 10, &w );
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
                rd = atoi(rd, &src, 10, uval );
				break;
			case 'd':
				val= va_arg( va, int *);
                rd = atoi(rd, &src, 10, val );
				break;
			case 'x':
				val= va_arg( va, int *);
                rd = atoi(rd, &src, 16, val );
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

/** 
 * The following functions have been borrowed from bwio and 
 * renamed. We did not write them ourselves.
 */
int atod( char ch ) {
	if( ch >= '0' && ch <= '9' ) return ch - '0';
	if( ch >= 'a' && ch <= 'f' ) return ch - 'a' + 10;
	if( ch >= 'A' && ch <= 'F' ) return ch - 'A' + 10;
	return -1;
}

char atoi( char ch, char **src, int base, int *nump ) {
	int num, digit, sign;
	char *p;

	p = *src; num = 0; sign = 1;
    if( ch == '-' ) {
        sign = -1;
        ch = *p++;
    }
	while( ( digit = atod( ch ) ) >= 0 ) {
		if ( digit > base ) break;
		num = num*base + digit;
		ch = *p++;
	}
	*src = p; *nump = num * sign;
	return ch;
}

void uitoa( unsigned int num, unsigned int base, char *bf ) {
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

void itoa( int num, char *bf ) {
	if( num < 0 ) {
		num = -num;
		*bf++ = '-';
	}
	uitoa( num, 10, bf );
}

char ctox( char ch ) {
	if ( (ch <= 9) ) return '0' + ch;
	return 'a' + ch - 10;
}
