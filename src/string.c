/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#define	DEBUG 1
#include "string.h"
#include "servers.h"
#include "requests.h"
#include "debug.h"

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

// _sscanf -- sscanf helper
int _sscanf ( const char *src, const char *fmt, va_list va ) {
	char ig, *dest;
	int w, *val, ret=0;
	unsigned int *uval;

	for ( ; *fmt && *src; fmt++ ) {
		if ( *fmt != '%' ) {
			if( *fmt != *src )	return -1;
			src++;
		} else {
			ig =0; w = 0; fmt++;

			if ( *fmt == '*' ) {
				ig = 1; fmt++; // TODO ignore it not implemented
			}
			if( *fmt >= '1' && *fmt <= '9' ) {
				w = atoi( &fmt, 10 );
			}
			switch( *fmt ) {
			  case 'c':
				*(va_arg( va, char* ) ) = *src;
				src++;
				break;
			  case 's':
				dest = va_arg( va, char* );
				if( ws(*src) ) return -5;
				while( !ws(*src) ) { // stop at whitespace
					*dest++ = *src++;
				}
				*dest = '\0'; // terminate the destination string
				break;
			  case 'u':
				if( atod( *src ) < 0 ) return -7;
				uval =	va_arg( va, unsigned int *);
				*uval = atoi( &src, 10 );
				break;
			  case 'd':
				//if( atod( *src ) < 0 ) return -8; //does not handle '-'
				val= va_arg( va, int *);
				*val = atoi( &src, 10 );
				break;
			  case 'x':
				if( atod( *src ) < 0 ) return -9;
				val= va_arg( va, int *);
				*val = atoi( &src, 16 );
				break;
			  case '%':
				if( *src != *fmt ) return -3;
				src++;
				break;
			}
			ret++;
		}
	}
	if( !*src && *fmt ) return -2; // we ran out of input
	return ret;
}

//sscanf
int sscanf( const char *src, const char *fmt, ... ) {
	int res;
	va_list va;
	
	va_start(va,fmt);
	res = _sscanf( src, fmt, va );
	va_end(va);

	debug("sscanf: res=%d, src='%s', fmt='%s'\r\n", res, src, fmt);
	return res;
}

// strcpyw -- strncpy with padding
char *strcpyw ( char *dest, const char * src, char fc, size_t w ) {
	size_t len = strlen( src ); 
	size_t pad = w - len;
//	debug( "strcpyw src:%s, fc:%c, w:%d, pad:%d\r\n", src, fc, w, pad );
	// Pad the string beginning
	if( pad > 0 ) { dest = memoryset( dest, fc, pad ); }
	// Copy the source into the destination
	return strncpy( dest, src, len );
}

// _sprintf -- printf's helper
size_t _sprintf ( char *str, const char *fmt, va_list va ) {
	char bf[12];
	char lz, *s= str;
	size_t w;
	
	for ( ; *fmt ; fmt++ ) {
		if ( *fmt != '%' ) {
			*s++ = *fmt;
		} else {
			lz = ' '; w = 0; fmt++;	// get the next character after '%'
			if ( *fmt == '0' ) {
				lz = '0'; fmt++;
			}
			if( *fmt >= '1' && *fmt <= '9' ) {
				w = atoi( &fmt, 10 );
			}
			switch( *fmt ) {
			  case 0: return(s - str);
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
				*s++ = *fmt;
				break;
			}
		}
	}
	return(s - str);
}

// sprintf
size_t sprintf ( char * str, const char * fmt, ... ) {
	int len;
	va_list va;
	
	va_start(va,fmt);
	len = _sprintf( str, fmt, va );
	va_end(va);

	return len;
}
// printf
size_t cprintf ( int iosTid, const char * fmt, ... ) {
	int			err;
	char 		reply;
	IORequest	req;
	va_list 	va;
	
	debug( "printf: format '%s'\r\n", fmt );
	va_start(va,fmt);
	req.len = _sprintf( req.data, fmt, va );
	va_end(va);
	debug( "printf: printing len=%d '%s' \r\n", req.len, req.data );
	
	req.type    = PUTSTR;
	assert( req.len < sizeof(req.data) );

	err = Send(iosTid, (char*)&req, IO_REQUEST_SIZE + req.len,
			(char*)&reply, sizeof(char));
	if( err < NO_ERROR ) return error;
	return req.len;
}

/********************************************
 *				HELPER FUNCTIONS 			*
 ********************************************/

/** 
 * The following functions have been borrowed from bwio modified and 
 * renamed. We did not write them ourselves.
 */
int atod( char ch ) {
	if( ch >= '0' && ch <= '9' ) return ch - '0';
	if( ch >= 'a' && ch <= 'f' ) return ch - 'a' + 10;
	if( ch >= 'A' && ch <= 'F' ) return ch - 'A' + 10;
	return -1;
}

int atoi( const char **src, int base ) {
	debug( "atoi: src='%s', base=%d\r\n", *src, base );
	int num = 0, digit, sign = 1;
	const char *s = *src;

	if( *s == '-' ) {
		sign = -1;
		s++;
	}
	while( ( digit = atod( *s ) ) >= 0 ) {
		if ( digit > base ) break;
		num = num*base + digit;
		s++;
	}
	*src = s;

	debug( "\tatoi: newsrc='%s', result=%d\r\n", *src, num*sign );
	return (num * sign);
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
