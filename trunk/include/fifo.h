/**
 * CS 452
 * becmacdo
 * dgoc
 */

#ifndef __FIFO_H__
#define __FIFO_H__

typedef struct fifo Fifo;

// Initialize the Fifo
void fifo_init ( Fifo *fifo );

// Pop off front
int fifo_pop ( Fifo *fifo, int value );

// Append to tail
void fifo_push ( Fifo *fifo );

// Ascending ordered Insert
void fifo_insertA ( Fifo *fifo, int value );

// Descending ordered insert
void fifo_insertD ( Fifo *fifo, int value );

#endif
