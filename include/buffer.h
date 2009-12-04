/**
 * CS 452
 * becmacdo
 * dgoc
 */

#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <string.h>
#include <globals.h>


struct ringBuffer {
	char *buffer;
	size_t bufSize;
	size_t eltSize;
	int end;
	int start;
	int size;
}; // Ring buffer

typedef struct ringBuffer RB;	// Ring buffer

#define rb_init( rb, buffer ) \
__rb_init( rb, buffer, sizeof ( buffer[0] ), array_size( buffer ) )
void	__rb_init ( RB *, void *, size_t eltSize, int num ) ;
void 	rb_push( RB *, void * ) ;
void 	rb_force( RB *, void * ); 
int 	rb_empty( RB * );
int 	rb_full( RB * );
void   *rb_pop( RB * );
void   *rb_top( RB * );


#endif
