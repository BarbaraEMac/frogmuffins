/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#ifndef __STRING_H__
#define __STRING_H__

// ****************************************************************
// * NOTE: Most descriptions copied from the standard c libraries *
// ****************************************************************

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
char * strncpy ( char *destination, const char * source, size_t num ) ;
	
/*
 * Copy block of memory
 *
 * Copies the values of num bytes from the location pointed by source
 * directly to the memory block pointed by destination.
 * 
 * The underlying type of the objects pointed by both the source 
 * and destination pointers are irrelevant for this function; 
 * The result is a binary copy of the data.
 *
 * The function does not check for any terminating null character in source 
 * - it always copies exactly num bytes.
 *
 * To avoid overflows, the size of the arrays pointed by both the destination
 * and source parameters, shall be at least num bytes, and should not overlap 
 * (for overlapping memory blocks, memmove is a safer approach).
 */
char * memcpy ( char * destination, const char * source, size_t num ) ;

/**
 * Compare two strings
 *
 * Compares the C string str1 to the C string str2.
 * This function starts comparing the first character of each string.
 * If they are equal to each other, it continues with the following pairs 
 * until the characters differ or until a terminanting null-character is reached.
 */
int strcmp (const char *str1, const char *str2);

/**
 * Parse a string given the format.
 */
int sscanf( char *src, char *fmt, ... );

/**
 * The following functions were borrowed and renamed from bwio.
 * We did not write them ourselves.
 */

/**
 * Convert the given character into an integer.
 */
int atod( char ch );

/**
 * Convert the given string to into an integer.
 */
char atoi( char ch, char **src, int base, int *nump );

/**
 * Converts a given unsigned integer into a string.
 * The base can be specified.
 */
void uitoa( unsigned int num, unsigned int base, char *bf );

/**
 * Converts a given integer into a string.
 * num - The integer to convert
 * bf  - A buffer that we copy the returned string into.
 */
void itoa( int num, char *bf );

/**
 * Converts a character into its hexidecimal value.
 * ch - The character to convert
 * Returns: The hexidecimal value of the character.
 */
char ctox( char ch );

#endif
