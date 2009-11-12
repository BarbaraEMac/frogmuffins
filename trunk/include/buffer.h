/**
 * CS 452
 * becmacdo
 * dgoc
 */

#ifndef __BUFFER_H__
#define __BUFFER_H__

#include <string.h>


struct ringBuffer {
	char *buffer;
	size_t bufSize;
	size_t eltSize;
	int end;
	int start;
	int size;
}; // Ring buffer

typedef struct ringBuffer RB;	// Ring buffer


void 	rb_init( RB *, void *, size_t eltSize, int num ) ;
void 	rb_push( RB *, void * ) ;
int 	rb_full( RB * );
int 	rb_empty( RB * );
void   *rb_pop( RB * );


#endif
