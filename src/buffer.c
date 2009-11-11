/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#define DEBUG 1

#include "debug.h"
#include "error.h"
#include "buffer.h"

#define NUM_ENTRIES	64

struct ringBuffer {
	Element buffer[NUM_ENTRIES];
	int end;
	int start;
	int size;
}; // Ring buffer

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

void rb_init ( RB *rb ) {

	// The fifo starts empty
	rb->size = 0;
	rb->end = 0;
	rb->start = 0;
}

void rb_push( RB *rb, Element el ) {
	assert( rb->size < NUM_ENTRIES );

	// Insert the element
	rb->buffer[rb->end++] = el;
	rb->end %= NUM_ENTRIES;

	// Update the size
	rb->size++;
}

Element rb_pop( RB *rb ) {
	assert( rb->size > 0 );

	// Remove the element
	Element el = rb->buffer[rb->start++];
	rb->start %= NUM_ENTRIES;

	// Update the size
	rb->size--;

	return el;
}
