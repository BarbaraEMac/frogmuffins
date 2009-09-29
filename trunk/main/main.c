/*
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */

#include <bwio.h>
#include <ts7200.h>

#include "switch.h"
#include "requests.h"
#include "td.h"

#define FOREVER     for( ; ; )
#define WAIT        for( i=0; i<200000; i++) {}
#define NUM_TDs     64


/*
// the following funciton was copied and modified from wikipedia
int rev_log2(unsigned char x) {
  int r = 0;
  while ((x >> r) != 0) { r++; }
  return 9-r; // returns -1 for x==0, floor(log2(x)) otherwise
}

// zero-fill a char array
void charset( char*str, int len, char ch=0 ) {
    while( (--len) >= 0 ) str[len] = ch;
}*/



void test( ) {
	for( ;; ) {
		bwputstr( COM2, "Task ending.\r\n" );
		asm( "swi" );
		bwputstr( COM2, "Task starting\r\n" );
	}
}

void initialize ( TD *tds ) {
    // Init things to zero?


}

TD * getNextRequest ( TD *td ) {
    TD *newTd;

    return newTd;
}

void service ( TD *td, Request *nxt ) {
    
    // Determine the request type and handle the call
    switch (nxt->type) {
        case CREATE:

            break;
        case MYTID:


            break;
        case MYPARENTTID:



            break;

        case PASS:

            break;
        case EXIT:
            

            break;

    }


}

TD * schedule () {
    // Schedule the next task to run?
    // Probably do some fun scheduling algorotihm here


}

int main( int argc, char* argv[] ) {

    TD taskDescriptors [64];
    TD *active;
    Request *nextRequest;

    // This is what we will end up with.
    // Look, it's already done.
/*
    initialize ( taskDescriptors );

    FOREVER {
        nextRequest = getNextRequest (active);
        service (active, nextRequest);
        active = schedule ();
    }
*/

    int data, *junk;
	int i;
	// Set up the Software interrupt for context switches
	int *swi = (int *) 0x28;
	*swi = (int) &kerEnt;


	// Set up the first task
	TD task1 = { 0x00000010, 0x21B000-0x24, &test };
	junk = 0x21AFFC;
	*junk = &test;
	Request r1 = {2, 3, 4, 5};
	
    data = (int *) junk;
	
    bwsetfifo( COM2, OFF );
    bwputstr( COM2, "Page table base: " );
    bwputr( COM2, data );
    bwputstr( COM2, "\r\n" );
    bwputr( COM2,  (int) &test);
	for( i=0; i<8; i++ ) {
		bwprintf( COM2, "Going into a context switch sp=%x spsr=%x\r\n", task1.sp, task1.spsr );
		kerExit(&task1, &r1);
		bwprintf( COM2, "Got back from context switch sp=%x spsr=%x\r\n", task1.sp, task1.spsr );
	}
    bwputstr( COM2, "Exiting normally" );
    return 0;
}
