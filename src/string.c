/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

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


