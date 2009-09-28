 /*
 * dgoc_a1.c
 */

#include <bwio.h>
#include <ts7200.h>
#include "switch.h"
#define FOREVER     for( ; ; )
#define WAIT        for( i=0; i<200000; i++) {}
#define INPUT_LEN   256
#define INPUT_HIST  1
#define NUM_TRNS    81
#define NUM_SWTS    256
#define SW_CNT      22
#define SW_WAIT     100
#define SNSR_WAIT   100
#define TRAIN_WAIT  500
#define TRAIN_TRIES 5

/*
int timeout = 250;

typedef struct {
    char box;
    int num;
    char trains[NUM_TRNS];
    char switches[NUM_SWTS];
    int* clock;
} globals;

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

void kerxit() {


}
void test( ) {
	for( ;; ) {
		bwputstr( COM2, "Task ending.\r\n" );
		asm( "swi" );
		bwputstr( COM2, "Task starting\r\n" );
	}
}

int main( int argc, char* argv[] ) {
//	char str[] = "Hello\n\r";
    // Initialize variables

    // Set up the communication ports
/*	bwsetfifo( COM2, OFF );
    bwsetspeed( COM1, 2400 );
	bwsetfifo( COM1, OFF );
    int *flag = (int *)( UART1_BASE + UART_LCRH_OFFSET );
    *flag |= 0x60 | STP2_MASK;
    */

    // Set up the timer
    int data, *junk;
	int i;
	// Set up the Software interrupt for context switches
	int *swi = (int *) 0x28;
	*swi = (int) &kerEnt;


	// Set up the first task
	TD task1 = { 0x00000010, 0x21B000, &test };
	junk = 0x21B020;
//	*junk = 0x21B000;
//	junk = 0x21B024;
	*junk = &test;
	Request r1 = {2, 3, 4, 5};
	
    data = (int *) junk;
	

    bwsetfifo( COM2, OFF );
    bwputstr( COM2, "Page table base: " );
    bwputr( COM2, data );
    bwputstr( COM2, "\r\n" );
    bwputr( COM2,  (int) &test);
	for( i=0; i<4; i++ ) {
		bwputstr( COM2, "Going into context switch\r\n" );
		kerExit(&task1, &r1);
		bwprintf( COM2, "Got back from context switch sp=%x spsr=%x\r\n", task1.sp, task1.spsr );
	}
    bwputstr( COM2, "Exiting normally" );
    return 0;

}


