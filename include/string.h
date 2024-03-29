/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#ifndef __STRING_H__
#define __STRING_H__

typedef char *va_list;

#define __va_argsiz(t)	\
		(((sizeof(t) + sizeof(int) - 1) / sizeof(int)) * sizeof(int))

#define va_start(ap, pN) ((ap) = ((va_list) __builtin_next_arg(pN)))

#define va_end(ap)	((void)0)

#define va_arg(ap, t)	\
		 (((ap) = (ap) + __va_argsiz(t)), *((t*) (void*) ((ap) - __va_argsiz(t))))


#define printf(args...) cprintf(WhoIs(SERIALIO2_NAME), args)
#define eprintf(args...) printf("\033[35m"); printf(args); printf("\033[37m")


// ****************************************************************
// * NOTE: Most descriptions copied from the standard c libraries *
// ****************************************************************

typedef int size_t;

typedef struct {
	char *start; //unused
	const char *curr;
	char *end;	//unused
	// function for reading
	// function for writing
} FILE;

/**
 * Has not been tested yet.
 * Concatenate strings
 * Appends a copy of the source string to the destination string. 
 * The terminating null character in destination is overwritten by the first
 * character of source, and a new null-character is appended at the end of 
 * the new string formed by the concatenation of both in destination.
 */
char * strconcat ( char *dest, const char *source, int destSize);

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
void * memcpy ( void * destination, const void * source, size_t num ) ;

/**
 * Compare two strings
 *
 * Compares the C string str1 to the C string str2.
 * This function starts comparing the first character of each string.
 * If they are equal to each other, it continues with the following pairs 
 * until the characters differ or until a terminanting null-character is 
 * reached.
 */
int strcmp (const char *str1, const char *str2);


/**
 * Get string length
 * 
 * Returns the length of str.
 */
size_t strlen ( const char * str );

/**
 * Read formatted data from string
 * 
 * Reads data from str and stores them according to the parameter format 
 * into the locations given by the additional arguments. 
 * Locations pointed by each additional argument are filled with 
 * their corresponding type of value specified in the format string.
 */
int sscanf( const char * src, const char * format, ... );

/**
 * Write formatted output to stream
 *
 * Writes to the specified stream a sequence of data formatted as the 
 * format argument specifies. After the format parameter,
 * the function expects at least as many additional arguments 
 * as specified in format.
 */
int fscanf( FILE *stream, const char * format, ... );


/**
 * Write formatted data to string
 *
 * Writes into the array pointed by str a C string consisting on a sequence of 
 * data formatted as the format argument specifies. After the format parameter, 
 * the function expects at least as many additional arguments as specified 
 * in format.
 */
size_t _sprintf ( char * str, const char * fmt, va_list va );
size_t sprintf ( char * str, const char * format, ... );

/**
 * Write formatted data to a COM port
 *
 * Sends to the COM server a C string consisting on a sequence of 
 * data formatted as the format argument specifies. After the format parameter, 
 * the function expects at least as many additional arguments as specified 
 * in format.
 */
size_t cprintf ( int iosTid, const char * format, ... ) ;

/**
 * The following functions were borrowed and renamed from bwio.
 * We did not write them ourselves.
 */

/**
 * Convert the given character into an integer.
 */
int atod( char ch );

/**
 * Convert the given string to into an integer stopping at whitespace.
 * Returns the number read, 0 if no match is found
 */
int atoi( const char **src );

/**
 * Converts a given unsigned integer into a string.
 * The base can be specified.
 * Returns: the nuber of characters written
 */
size_t uitoa( unsigned int num, unsigned int base, char *bf );

/**
 * Converts a given integer into a string.
 * num - The integer to convert
 * bf  - A buffer that we copy the returned string into.
 * Returns: the nuber of characters written
 */
size_t itoa( int num, char *bf );

/**
 * Converts a character into its hexidecimal value.
 * ch - The character to convert
 * Returns: The hexidecimal value of the character.
 */
char ctox( char ch );

/*
 * Fill block of memory
 * 
 * Sets the first num bytes of the block of memory pointed by
 * ptr to the specified value (interpreted as an unsigned char).
 *
 * Returns: place in memory after last set
 */
void *memoryset ( void *ptr, char value, size_t num );


/*
 * Checks for whitespace
 *
 * Returns true if the character passed in is whitespace
 */
inline int ws( const char ch );

#endif
