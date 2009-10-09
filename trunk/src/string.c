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

int strcmp (const char *str1, const char *str2) {
	while ( *str1 == *str2 && *(str1++) != 0 && *(str2++) != 0 ) {}
	
	if ( *str1 == 0 && *str2 == 0 ) {
		return 0;
	}

	return (*str1 < *str2) ? -1 : 1;
}
