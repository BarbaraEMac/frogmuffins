 /*
 * dgoc_a1.c
 */

#include <bwio.h>
#include <ts7200.h>
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

// check if the train index is within range
int train_check( char train ) {
    int res = ( train < NUM_TRNS );
    if( !res ) bwprintf( COM2, "Train #%d not in range.\n\r", train );
    return res;
}

// check if the direction index is within range
int dir_check( char dir, char *cmd ) {
    if( dir == 'c' || dir == 'C' ) { *cmd = 34; return 1; }
    if( dir == 's' || dir == 'S' ) { *cmd = 33; return 1; }
    bwprintf( COM2, "Train direction %c not in range.\n\r", dir );
    return 0;
}

// send commands to the train, try a few times
int train_send( char byte1, char byte2 ) {
    int i;
    for( i=0; i<TRAIN_TRIES; i++ ) {
        if( (bwsendc( COM1, byte1, TRAIN_WAIT ) == 0)  &&
            (bwsendc( COM1, byte2, TRAIN_WAIT ) == 0) )
                break;
    }
    wait( timeout );
    if( i==TRAIN_TRIES ) {
        bwputstr( COM2, "Sending to train: Connection timeout.\n\r" );
        return -1;
    }
    return 0;
}

// set a switch to a desired position checking for bad input
void switch_send( char sw, char dir, char* switches ) {
    char code; int res;
    if( dir_check( dir, &code ) ) {
        bwprintf( COM2, "Setting switch %d to %c.\n\r", sw, dir );
        res = bwsendc( COM1, code, SW_WAIT );
        bwsendc( COM1, sw, SW_WAIT );
        wait( SW_WAIT );
        bwsendc( COM1, 32, SW_WAIT );
        switches[(int) sw] = dir;    
        wait( SW_WAIT );
    }
}

// set all the switches to given direction
void switches_init( char dir, char* switches ) {
    char i;
    for( i=1; i<=18; i++ )      switch_send( i, dir, switches ); 
    for( i=153; i<=156; i++ )   switch_send( i, dir, switches ); 
}

// check for changes
void sensors_poll( globals *glob ) {
    bwclear( COM1 );                // clear the io before reading
    bwsendc( COM1, 133, SW_WAIT );
    bwsendc( COM1, 192, SW_WAIT );   // ask for sensor dump
    char ch=0; int i=-1, res;//, old_time=*(glob->clock);
    // look for the first non-empty char
    while( (++i<10) && (res = bwreadc( COM1, &ch, SNSR_WAIT)) && !ch ) {} 

    if( !res  ) bwputstr( COM2, "Reading: Connection Timeout" );
    if( ch && res ) {
        glob->box = 'A' + (i / 2);
        glob->num = rev_log2( ch );
        if( i % 2 ) glob->num +=8;
     // bwprintf( COM2, "The sensor triggered was %c%d.\n\r", x->box, x->num );
    }
    //bwprintf( COM2, "\n\rPoll time: %d\n", old_time - *(glob->clock) );
    bwclear( COM1 );                // ignore the rest of the input
}
// execute the command passed in
int exec( char *command, globals *glob ) {
    int sw, tr, spd, mask;
    char dir;
    
    if ( sscanf(command, "q\r") >= 0 ) {
        bwprintf( COM2, "Quitting.\n\r" );
        return -1;
        
    } else if( sscanf(command, "tr %d %d", &tr, &spd) >= 0 ) {
        if( train_check( tr ) ) {
            bwprintf( COM2, "Setting train #%d to speed %d.\n\r", tr, spd );
            train_send( (char) spd, (char) tr );
            glob->trains[tr] = (char) spd;
        }
    
    } else if( sscanf(command, "rv %d", &tr) >=0 ) {
        if( train_check( tr ) ) {
            spd = glob->trains[tr];
            bwprintf( COM2, "Reversing train #%d to speed %d.\n\r", tr, spd );
            train_send( 0, (char) tr );
            train_send( 15, (char) tr );
            train_send( (char) spd, (char) tr );
        }
    } else if( sscanf(command, "sw %d %c", &sw, &dir) >=0 ) {
        switch_send( (char) sw, (char) dir, glob->switches );
    
    } else if( sscanf(command, "wh\r") >=0 ) {
        if( glob->box == 'X' ) bwputstr( COM2, "No sensor triggered yet\n\r" );
        else bwprintf( COM2, "Last sensor triggered was %c%d.\n\r", glob->box, glob->num );

    } else if( sscanf(command, "st %d", &sw) >=0 ) {
        bwprintf( COM2, "Switch %d is set to %c.\n\r", sw, glob->switches[sw] );

    } else if( sscanf(command, "asw %c", &dir) >=0 ) {
        switches_init( dir, glob->switches );

    } else if( sscanf(command, "wa %d\r", &timeout) >=0 ) {
        bwprintf( COM2, "Setting the wait to %d ms.\n\r", timeout );
    
    } else if( sscanf(command, "ua %x\r", &mask) >=0 ) {
        bwprintf( COM2, "Changing UART1 to %04x.\n\r", mask );
        int *flag = (int *)( UART1_BASE + UART_LCRH_OFFSET );
        *flag = mask;
        bwprintf( COM2, "UART1 SETTINGS: 0x%04x\n\r", *flag );
    
    }

    return 0;
}

// zero-fill a char array
void reset( char*str, int len ) {
    while( (--len) >= 0 ) str[len] = 0;
}

int trainTask( ) {
//	char str[] = "Hello\n\r";
    // Initialize variables
    char *input, history[INPUT_HIST][INPUT_LEN];
    globals glob; glob.box = 'X'; glob.num = 99;
    timeout = 250;              // better set this again
    reset( glob.trains, NUM_TRNS );
    reset( glob.switches, NUM_SWTS );

    // Set up the communication ports
	bwsetfifo( COM2, OFF );
    bwsetspeed( COM1, 2400 );
	bwsetfifo( COM1, OFF );
    int *flag = (int *)( UART1_BASE + UART_LCRH_OFFSET );
    *flag |= 0x60 | STP2_MASK;

    // Set up the timer
    glob.clock = clock_init(TIMER3_BASE, 1, 0 );

    bwprintf( COM2, "\n\rsw_count=%d\n\r", SW_CNT );
    // Main loop
    int i=0, h=0, time, tens, secs, mins, old_time;
    input = history[h++];
    FOREVER {

        input[i] = 0;                       // clear the next character
        if( old_time >= *(glob.clock) + 200 ) {
            time = *(glob.clock) / -200;
            tens = time % 10;
            secs = (time / 10) % 60;
            mins = time / 600;
            bwprintf( COM2, "\r%02d:%02d.%d> %s", mins, secs, tens, input );
            old_time = *(glob.clock);
            sensors_poll( &glob );             // poll the sensors
        }
        if( bwreadc( COM2, &(input[i]), 0 ) ) {
            if( input[i] == '\r' ) {        // Enter was pressed
                bwprintf( COM2, "\n\r" );
                input[i+1] = 0;
                if( exec( input, &glob ) == -1) return 0;
                
                // Clear the input for next line
                //input = history[h++];
                //h++; h &= INPUT_HIST;
                i=0;
            } else if( input[i] == '\b'
                    || input[i] == 127) {    // Backspace was pressed
                bwputc( COM2, '\b' );                    
                bwputc( COM2, ' ' );                    
                input[i] = 0;
                i--; if( i<0 ) i=0;
            } else {                        // update the position in the command string
                i++;
            }
            //bwprintf( COM2, "\n\rCODE(%d)\n\r", input[i-1] );
        }
        if( i == INPUT_LEN ) {
            bwprintf( COM2, "too many characters.\r\n" );
            i=0;
        }

    }
	return 0;
}


