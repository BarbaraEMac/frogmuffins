/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#define DEBUG 1

#include "debug.h"
#include "error.h"
#include "buffer.h"

// rb_init
void 
__rb_init ( RB *rb, void * buffer, size_t eltSize, int num ) {

	debug("rb_init: rb=%x, bf=%x, eltSize=%d, num=%d\r\n",
			rb, buffer, eltSize, num );
	// The fifo starts empty
	rb->size = 0;
	rb->end = 0;
	rb->start = 0;
	rb->buffer = buffer;
	rb->bufSize = num * eltSize;
	rb->eltSize = eltSize;

	assert( (rb->bufSize % rb->eltSize) == 0 );
}

// rb_push
void 
rb_push( RB *rb, void *el ) {
	debug( "rb_push: rb=%x, el=%x, start=%d, end=%d\r\n", 
			rb, el, rb->start, rb->end );
	assert( !rb_full( rb ) );

	// Insert the elements
	memcpy( (rb->buffer + rb->end), el, rb->eltSize );
	rb->end += rb->eltSize;
	rb->end %= rb->bufSize;

	// Update the size
	rb->size += rb->eltSize;
}

// rb_pop
void * 
rb_pop( RB *rb ) {
	debug( "rb_pop: rb=%x, start=%d, end=%d\r\n", rb, rb->start, rb->end );
	assert( !rb_empty( rb ) );

	// Remove the element
	void *el = (rb->buffer + rb->start);
	rb->start += rb->eltSize;
	rb->start %= rb->bufSize;

	// Update the size
	rb->size -= rb->eltSize;

	return el;
}


// rb_force
void 
rb_force( RB *rb, void *el ) {
	// remove the last element if full
	if( rb_full( rb ) ) 
		rb_pop( rb );
	// do the push
	rb_push( rb, el );
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

