/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#ifndef __STRING_H__
#define __STRING_H__


typedef int size_t;

/*
 * Copy characters from string
 *
 * Copies the first num characters of source to destination. 
 * If the end of the source C string (which is signaled by a null-character) 
 * is found before num characters have been copied, 
 * destination is padded with zeros until a total of num characters 
 * have been written to it.
 */
char * strncpy ( char *destination, const char * source, size_t num ) {
	// TODO copy this to string.c
	int i;
	for ( i=0; (i < num) && source[i]; i++ ) {
		destination[i] = source[i];
	}
	for ( ; i < num; i++ ) {
		destination[i] = '\0';
	}
	return destination;
}

#endif
