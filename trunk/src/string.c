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

// strcat
char * strconcat ( char *dest, const char *source, int destSize) {
	int destLen   = strlen(dest);
	int sourceLen = strlen(source);
	int totalLen  = destLen + sourceLen;

	assert ( destSize <= totalLen );

	int i;
	for ( i = 0 ; i < sourceLen; i ++ ) {
		dest[ destLen + i - 1 ] = source[i];
	}
	dest[totalLen] = '\0';

	return dest;
}

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
void * memcpy ( void * destination, const void * source, size_t num ) {
	char *s = (char *) source;
	char *d = (char *) destination;
	while( (--num) >= 0 ) d[num] = s[num];
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
int _fscanf ( FILE *stream, const char *fmt, va_list va ) {
	char ig, *dest;
	const char *s = stream->curr;
	int w, *val, ret = 0;
	unsigned int *uval;

	for ( ; *fmt && *s; fmt++ ) {
		if ( *fmt != '%' ) {
			if( ws(*fmt) ) {
				if( !ws(*s) ) return -2;
				while( ws(*s) ) { s++; }
			} else if( *fmt != *s ) {
				return -1;
			} else {
				s++;
			}
		} else {
			ig =0; w = 0; fmt++;

			if ( *fmt == '*' ) {
				ig = 1; fmt++; // TODO ignore it not implemented
			}
			if( *fmt >= '1' && *fmt <= '9' ) {
				w = atoi( &fmt );
			}
			switch( *fmt ) {
			  case 'c':
				*(va_arg( va, char* ) ) = *s;
				s++;
				break;
			  case 's':
				dest = va_arg( va, char* );
				if( ws(*s) ) return -5;
				while( !ws(*s) ) { // stop at whitespace
					*dest++ = *s++;
				}
				*dest = '\0'; // terminate the destination string
				break;
			  case 'u':
				if( atod( *s ) < 0 ) return -7;
				uval =	va_arg( va, unsigned int *);
				*uval = atoi( &s );
				break;
			  case 'd':
			  case 'x':
				if( atod( *s ) < 0 ) return -8; //does not handle '-'
				val= va_arg( va, int *);
				*val = atoi( &s );
				break;
			  case '%':
				if( *s != *fmt ) return -3;
				s++;
				break;
			}
			ret++;
		}
	}
	if( !*s && *fmt ) return -2; // we ran out of input
	stream->curr = s; // update the curr pointer
	return ret;
}

//sscanf
int sscanf( const char *src, const char *fmt, ... ) {
	int res;
	va_list va;
	FILE f;
	f.curr = src;
	
	va_start(va,fmt);
	res = _fscanf( &f, fmt, va );
	va_end(va);

	debug("sscanf: res=%d, fmt='%s', src='%s'\r\n", res, fmt, src);
	return res;
}

//fscanf
int fscanf( FILE *stream, const char * fmt, ... ) {
	int res;
	va_list va;
	
	va_start(va,fmt);
	res = _fscanf( stream, fmt, va );
	va_end(va);

	debug("fscanf: res=%d, fmt='%s', src='%s'\r\n", res, fmt, stream->curr);
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
				w = atoi( &fmt );
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

int atoi( const char **src ) {
	debug( "atoi: src='%s'\r\n", *src );
	int num = 0, digit, sign = 1, base = 10;
	const char *s = *src;

	if( *s == '-' ) { 
		sign = -1;
		s++;
	} else if( s[0] == '0' && s[1] == 'x' ) {
		base = 16;
		s += 2;
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
		len = 1;
	}
	return len + uitoa( num, 10, bf );
}

char ctox( char ch ) {
	if ( (ch <= 9) ) return '0' + ch;
	return 'a' + ch - 10;
}

// value-fill a char array
void *memoryset( void *str, char value, size_t num ) {
	char *s = (char *) str;
	while( num-- > 0 ) *s++ = value;
	return s;
}

// check for whitespace
inline int ws( const char ch ) {
	return (ch <= 32 || ch >= 127);
}
