/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#define DEBUG 1

#include <ts7200.h>

#include "debug.h"
#include "error.h"
#include "fifo.h""

#define NUM_ENTRIES	64
#define	UNUSED_TID		-1

//-----------------------------------------------------------------------------

typedef struct entry Entry;

struct entry {
	int value;

	Entry *next;	// Pointer to next entry in the queue
	Entry *prev;	// Pointer to the previous entry in the queue
};

struct fifo {
	Entry unused[NUM_ENTRIES];
	int ptr;

	Entry *head;
};

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

void fifo_init ( Fifo *fifo ) {

	// The fifo starts empty
	fifo->head = 0;

	// Set the ptr to the next unused Entry elem to 0.
	fifo->ptr = 0;
}

void fifo_insertD ( Fifo *fifo, int value ) {
	Entry *head = fifo->head;
	Entry toAdd = fifo_newEntry (fifo, value);

	assert( head != 0 );
	assert( toAdd != 0 )

	// The empty list
	if ( *head == 0 ) {
		debug ("	fifo: add %d %d as head \r\n", toAdd->tid,toAdd->value);
		toAdd->next = toAdd;
		toAdd->prev = toAdd;

		*head = toAdd;
	}
	// New Element is the new head
	else if ( toAdd->value < (*head)->value ) {
		Entry *oldHead = *head;
		Entry *tail    = oldHead->prev;

		debug("		fifo: add as new head %d %d (old: %d %d) \r\n", toAdd->tid, toAdd->value, oldHead->tid, oldHead->value);
		toAdd->next = oldHead;
		toAdd->prev = tail;

		oldHead->prev = toAdd;
		tail->next    = toAdd;
		
		// This is the new head!
		*head = toAdd;
	}
	// The new element needs to be inserted in the middle
	else {
		Entry *itr = (*head)->next;
		while ( toAdd->value > itr->value && itr != *head ) {
			assert ( itr != 0 );
			assert ( itr->next != 0 );
			assert ( itr->prev != 0 );
			
			itr = itr->next;
		}
	
		debug("		fifo: add in middle to: %d %d prev: %d %d, next: %d %d\r\n", toAdd->tid, toAdd->value, itr->prev->tid, itr->prev->value, itr->prev->next->tid, itr->prev->next->value);
		// Insert in the middle
		toAdd->next = itr;
		toAdd->prev = itr->prev;

		itr->prev->next = toAdd;
		itr->prev       = toAdd;
	}


	debug ("\tfifo: added tid: %d value: %d\r\n", toAdd->tid, toAdd->value);

	assert( *head != 0 );
	assert( toAdd->next != 0 );
	assert( toAdd->prev != 0 );
}

int fifo_removeValue ( Fifo *fifo, int value  ) {
	debug ("\tfifo: removing value: %d\r\n", >value);
	
	// Remove the head - this is the last element
	if ( *head == toRm ) {
		Entry *newHead = toRm->next;
		Entry *tail = toRm->prev;

		// This is the last element
		if ( newHead == toRm ) {
			debug ("\tfifo: removing last element (head).\r\n");
			*head = 0;	
		}
		else {
			debug ("\tfifo: removing head. Adding next as new head.\r\n");
			newHead->prev = tail;
			tail->next    = newHead;

			*head = newHead;
		}
	}
	// Remove another element, not the head
	else {
		debug("\tfifo: removing middle element\r\n");
		Entry *next = toRm->next;
		Entry *prev = toRm->prev;

		next->prev = prev;
		prev->next = next;
	}

	// Reset these links just incase something screwy happens
	toRm->next = 0;
	toRm->prev = 0;
	toRm->tid = UNUSED_TID;
}
