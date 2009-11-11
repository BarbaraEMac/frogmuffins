/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#define DEBUG 1

#include "debug.h"
#include "error.h"
#include "buffer.h"


//-----------------------------------------------------------------------------

// Initialize the Fifo
// // // I might delete this ..
// void fifo_init ( Fifo *head );
//
// // Pop off front
// int fifo_pop ( Fifo *head, int elem );
//
// // Append to tail
// void fifo_push ( Fifo *head );
//
// // Ascending ordered Insert
// void fifo_insertA ( Fifo *head, int elem );
//
// // Descending ordered insert
// void fifo_insertD ( Fifo *head, int elem );
//

// rb_init
void 
rb_init ( RB *rb, void * buffer, size_t bufSize, size_t eltSize ) {

	// The fifo starts empty
	rb->size = 0;
	rb->end = 0;
	rb->start = 0;
	rb->buffer = buffer;
	rb->bufSize = bufSize;
	rb->eltSize = eltSize;

	assert( (bufSize % eltSize) == 0 );
}

// rb_push
void 
rb_push( RB *rb, void *el ) {
	assert( !rb_full( rb ) );

	// Insert the elements
	memcpy( rb->buffer+rb->start, el, rb->eltSize );
	rb->end += rb->eltSize;
	rb->end %= rb->bufSize;

	// Update the size
	rb->size += rb->eltSize;
}

// rb_pop
void * 
rb_pop( RB *rb ) {
	assert( !rb_empty( rb ) );

	// Remove the element
	void *el = &rb->buffer[rb->start];
	rb->start += rb->eltSize;
	rb->start %= rb->bufSize;

	// Update the size
	rb->size -= rb->eltSize;

	return el;
}

// rb_full
int	
rb_full( RB *rb ) {
	return (rb->size >= rb->bufSize - rb->eltSize); // last element stays in
}

// rb_empty
int	
rb_empty( RB *rb ) {
	return (rb->size <= 0);
}

