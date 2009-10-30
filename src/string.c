/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include "string.h"

// strncpy
char * strncpy ( char *dest, const char * src, size_t num ) {
	// Copy the source into destination
	for ( ; (num > 0) && *src; num-- ) 
		*dest++ = *src++;
	// Padd the rest with zeroes if needed
	for ( ; (num > 0); num-- ) 
		*dest++ = '\0';
	return dest;
}

// memcpy
char * memcpy ( char * destination, const char * source, size_t num ) {
	while( (--num) >= 0 ) destination[num] = source[num];
	return destination;
}

// strcmp -- implementation from wikipedia.org
int strcmp (const char *s1, const char *s2) {
	for(; *s1 == *s2; ++s1, ++s2)
		if(*s1 == 0)
			return 0;
	return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

// strlen -- implementation from wikipedia.org
inline size_t strlen ( const char * str ) {
	const char * s = str;
	for (; *s; ++s);
	return(s - str);
}

// scan -- sscanf helper
int scan ( const char *src, const char *fmt, va_list va ) {
	char ch, ig, rd, *dest;
	int w, *val;
	unsigned int *uval;

	rd = *(src++);	
	while ( ( ch = *(fmt++) ) && rd ) {
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
			switch( ch ) {
			case 'c':
				*(va_arg( va, char* ) ) = rd;
				rd = *(src++);
				break;
			case 's':
				dest = va_arg( va, char* );
				if( ws(rd) ) return -5;
				while( !ws(rd) ) { // stop at whitespace
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
		}
	}
	if( !rd && ch ) return -2; // we ran out of input
	return 0;
}

//sscanf
int sscanf( const char *src, const char *fmt, ... ) {
	int res;
	va_list va;
	
	va_start(va,fmt);
	res = scan( src, fmt, va );
	va_end(va);

	return res;
}

// strcpyw -- strncpy with padding
char *strcpyw ( char *dest, const char * src, char fc, size_t w ) {
	size_t len = strlen( src ); 
	size_t pad = w - len;
	// Pad the string beginning
	if( pad > 0 ) 
		src = memoryset( dest, fc, pad );
	// Copy the source into the destination
	return strncpy( dest, src, len );
}

// format -- printf's helper
size_t format ( char *str, const char *fmt, va_list va ) {
	char bf[12];
	char ch, lz, *s= str;
	size_t w, len=0; // len holds the length of the sting generated so far
	
	while ( ( ch = *(fmt++) ) ) {
		if ( ch != '%' ) {
			str[len++] = ch;
		} else {
			lz = ' '; w = 0;
			ch = *(fmt++);
			if ( ch == '0' ) {
				lz = '0'; ch = *(fmt++);
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
			switch( ch ) {
			case 0: return len;
			case 'c':
				*s++ = va_arg( va, char );
				break;
			case 's':
				s = strcpyw( s, va_arg( va, char* ), ' ', w );
				break;
			case 'u':
				uitoa( va_arg( va, unsigned int ), 10, bf );
				s = strcpyw( s, bf, lz, w );
				break;
			case 'd':
				itoa( va_arg( va, int ), bf );
				s = strcpyw( s, bf, lz, w );
				break;
			case 'x':
				uitoa( va_arg( va, unsigned int ), 16, bf );
				s = strcpyw( s, bf, lz, w );
				break;
			case '%':
				*s++ = ch;
				break;
			}
		}
	}
	return s-str;
}

// sprintf
size_t sprintf ( char * str, const char * fmt, ... ) {
	int len;
	va_list va;
	
	va_start(va,fmt);
	len = format( str, fmt, va );
	va_end(va);

	return len;
}

/********************************************
 *				HELPER FUNCTIONS 			*
 ********************************************/

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

char atoi( char ch, const char **src, int base, int *nump ) {
	int num, digit, sign;
	const char *p;

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

size_t uitoa( unsigned int num, unsigned int base, char *bf ) {
	size_t n = 0;
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
	return n;
}

size_t itoa( int num, char *bf ) {
	size_t len = 0;
	if( num < 0 ) {
		num = -num;
		*bf++ = '-';
		len=1;
	}
	return len + uitoa( num, 10, bf );
}

char ctox( char ch ) {
	if ( (ch <= 9) ) return '0' + ch;
	return 'a' + ch - 10;
}

// value-fill a char array
char *memoryset( char *str, char value, size_t num ) {
	while( num-- > 0 ) *str++ = value;
	return str;
}

// check for whitespace
inline int ws( const char ch ) {
	return (ch <= 32 || ch >= 127);
}
