/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#define DEBUG 1

#include <ts7200.h>

#include "debug.h"
#include "drivers.h"
#include "error.h"
#include "requests.h"
#include "servers.h"

#define DUMMY_TID		-1
#define DUMMY_CH		0xFF
#define AWAITBUF_LEN	1

//-----------------------------------------------------------------------------
// Private members & Forward Declarations

typedef struct {
	char sendBuffer[NUM_ENTRIES];
	char recvBuffer[NUM_ENTRIES];
	int waiting    [NUM_ENTRIES];	// Array of waiting tids (for a char)

	int sEmpIdx;
	int sFulIdx;
	int rEmpIdx;
	int rFulIdx;
	
	int wEmpIdx;					// Points to next EMPTY slot
	int wFulIdx;					// Points to the first FULL slot
} SerialIOServer;

/**
 * Initialize the Serial IO Server structure.
 */
void ios_init (SerialIOServer *server, UART *uart);

void ios_run (UART *uart);

void ios_attemptTransmit (SerialIOServer *ios, UART *uart);

void ionotifier_run();

int ionotifier_init();

//-----------------------------------------------------------------------------

inline void uart_write( UART *uart, char ch ) {
	uart->data = ch;
}

// Wrapper for UART1 / COM1 / Train Controller!
void ios1_run () {
	debug ("ios1: Running 1\r\n");
	
	int err = uart_install( UART1, 2400, OFF );
	if_error(err, "UART1 not installed.");
	
	debug ("ios1: installed the first driver.\r\n");

	// Start the server
	ios_run( UART1 );
}

// Wrapper for UART2 / COM2 / Monitor & Keyboard
void ios2_run () {
	debug ("ios2: Running 2\r\n");
	
	int err = uart_install( UART2, 115200, OFF );
	if_error(err, "UART2 not installed.");
	
	debug ("ios2: installed the second driver.\r\n");
	
	ios_run( UART2 );
}

// Advance the buffer pointer
void advance (int *ptr) {
	*ptr += 1;
	*ptr %= NUM_ENTRIES;
}

// Store the given value in the buffer and increment the pointer
void storeCh (char *buf, int *ptr, char ch) {
	buf[*ptr] = ch;
	advance(ptr);
	assert (*ptr <= NUM_ENTRIES);
}

void storeInt (int *buf, int *ptr, int item) {
	buf[*ptr] = item;
	advance (ptr);
}

void ios_run (UART *uart) {
	debug ("ios_run: The Serial IO Server is about to start. \r\n");	
	
	int			 senderTid;		// The task id of the message sender
	IORequest	 req;			// A serial io server request message
	int			 len;
	int			 i;
	SerialIOServer ios;

	// Initialize the Serial IO Server
	ios_init (&ios, uart);

	FOREVER {
		// Receive a server request
		debug ("ios: is about to Receive. \r\n");
		len = Receive ( &senderTid, (char *) &req, sizeof(IORequest) );
		
		debug ("ios: Received: Tid=%d  type=%d len=%d\r\n", 
				senderTid, req.type, len);
		
		assert( len >= IO_REQUEST_SIZE + req.len );
		assert( senderTid >= 0 );
		
		// Handle the request
		switch (req.type) {
			case NOTIFY_GET:
				debug ("ios: notified GET data=%c emp=%d ful=%d\r\n",
						req.data[0], ios.rEmpIdx, ios.rFulIdx);
				
				// Reply to the Notifier immediately
				Reply (senderTid, (char*) &req.data, sizeof(char));
				
				// If we have a character and someone is blocked, wake them up
				if ( ios.wEmpIdx != ios.wFulIdx ) {
					assert (ios.waiting[ios.wFulIdx] != DUMMY_TID);
					
					debug ("ios: waking up task %d to give ch=%c\r\n",
							ios.waiting[ios.wFulIdx], req.data[0]);
					Reply ((int)ios.waiting[ios.wFulIdx], (char *)&req.data, 
							sizeof(char));
					
					storeInt(ios.waiting, &ios.wFulIdx, DUMMY_TID);
				}
				
				// If we have a character and no one is waiting, store ch
				else {
					debug("ios: notified is storing ch=%c\r\n", req.data[0]);
					assert (ios.recvBuffer[ios.rEmpIdx] == DUMMY_CH);
					assert (ios.waiting[ios.wFulIdx] == DUMMY_TID);
					
					storeCh(ios.recvBuffer, &ios.rEmpIdx, req.data[0]);
				}
			
				break;

			case NOTIFY_PUT:
				debug ("ios: notified PUT emp=%d ful=%d\r\n", 
						ios.sEmpIdx, ios.sFulIdx);
				
				// Reply to the Notifier immediately
				Reply (senderTid, (char*) &req.data, sizeof(char));
				
				// Try to send the data across the UART!
				if ( ios.sFulIdx != ios.sEmpIdx ) {
					ios_attemptTransmit(&ios, uart);
				}
				break;
			
			case GETC:
				// If we have a character, send it back!
				//if (ios.recvBuffer[ios.rFulIdx] != DUMMY_CH) {
				if (ios.rFulIdx != ios.rEmpIdx ) {
					debug ("ios: getc has a char=%c ful=%d emp=%d wait:ful=%d emp=%d\r\n",
							ios.recvBuffer[ios.rFulIdx], ios.rFulIdx, ios.rEmpIdx,
							ios.wFulIdx, ios.wEmpIdx);
					assert (ios.recvBuffer[ios.rFulIdx] != DUMMY_CH);
					// Make sure no one else was waiting first ...
					assert (ios.wFulIdx == ios.wEmpIdx);	
					assert (ios.waiting[ios.wFulIdx] == DUMMY_TID);

					Reply (senderTid, (char *)&ios.recvBuffer[ios.rFulIdx], 
						   sizeof(char));
					
					// Empty the slot and advance the pointer
					storeCh(ios.recvBuffer, &ios.rFulIdx, DUMMY_CH);
				}
				// Otherwise, store the tid until we get a character
				else {
					debug ("ios: getc, but NO char ful=%d emp=%d wait:ful=%d emp=%d\r\n",
							ios.rFulIdx, ios.rEmpIdx, ios.wFulIdx, ios.wEmpIdx);
					assert (ios.waiting[ios.wEmpIdx] == DUMMY_TID);

					storeInt(ios.waiting, &ios.wEmpIdx, (char)senderTid);
				}

				break;
			
			case PUTC:
			case PUTSTR:
				debug ("ios: putstr str=%s len=%d\r\n", req.data, req.len);
				// Do not let the sender stay blocked for long.
				Reply (senderTid, (char*)&req.data, sizeof(char));

				// Copy the data to our send buffer
				for ( i = 0; i < req.len; i ++ ) {
				/*	if( ios.sendBuffer[ios.sEmpIdx] != DUMMY_CH ) {
						int k;
						for( k = 0; k < NUM_ENTRIES; k++ ) 
							bwprintf( COM2, "%d=[%d]\r\n", k, ios.sendBuffer[k]);
					}*/
					assert (ios.sendBuffer[ios.sEmpIdx] == DUMMY_CH);
					storeCh (ios.sendBuffer, &ios.sEmpIdx, req.data[i]); 
				}

				// Try to send some data across the UART
				ios_attemptTransmit (&ios, uart);

				break;
			
			default:
				error (IOS_INVALID_REQ_TYPE, 
					   "Serial IOserver request type is not valid.");
				break;
		}
	}
	
	Exit(); // This will never be called.
}

void ios_init (SerialIOServer *s, UART *uart) {
	int i, tid;
	char c;
	int notifierEvt = (uart == UART1) ? INT_UART1 : INT_UART2;

	// Create the notifier
	if ( (tid = Create (1, &ionotifier_run)) < NO_ERROR) {
		error (tid, "Cannot make IO notifier.\r\n");
	}
	
	// Send/Receive to synchronize with the notifier
	// This tells the notifier the server's tid & event to await
	Send ( tid, (char*) &notifierEvt, sizeof(int), 
			(char*) &notifierEvt, sizeof(int) );

	// Empty out the character buffers
	for (i = 0; i < NUM_ENTRIES; i++) {
		s->sendBuffer[i] = DUMMY_CH;
		s->recvBuffer[i] = DUMMY_CH;
		s->waiting[i]    = DUMMY_TID;
	}
	// Init array pointers
	s->sEmpIdx = 0;
	s->sFulIdx = 0;
	s->rEmpIdx = 0;
	s->rFulIdx = 0;
	s->wEmpIdx = 0;					
	s->wFulIdx = 0;				
	
	// Clear out the uart buffer
	while ( uart->flag & RXFF_MASK ) { c = (uart->data); }
	
	// Enable the UART
	uart->ctlr |= UARTEN_MASK;
	
	// Turn the interrupts ON
	uart->ctlr |= RIEN_MASK;
	uart->ctlr |= MSIEN_MASK;

	// Register with the name server
	if ( uart == UART1 ) {
		RegisterAs (SERIALIO1_NAME);
	} else {
		RegisterAs (SERIALIO2_NAME);
	}
}

void ios_attemptTransmit (SerialIOServer *ios, UART *uart) {
	debug("ios: attempting to transmit ful=%d emp=%d ch=%c\r\n", 
		   ios->sFulIdx, ios->sEmpIdx, ios->sendBuffer[ios->sFulIdx]);
	
	// If the transmitter is busy, wait for interrupt
	if ( uart->flag & TXBUSY_MASK ) {
		debug ( "ios: transmit busy. Turning on transmit interrupt.\r\n");
		uart->ctlr |= TIEN_MASK;	// Turn on the transmit interrupt
	
	// Otherwise, write a single byte
	} else if( (uart->flag & CTS_MASK) || (uart != UART1) ) {	
		debug ("writing %c to uart\r\n", ios->sendBuffer[ios->sFulIdx]);

		uart_write ( uart, ios->sendBuffer[ios->sFulIdx] );
	
		// Clear buf & advance pointer
		storeCh (ios->sendBuffer, &ios->sFulIdx, DUMMY_CH);

		// If we want to send more, enable the interrupt
		if ( ios->sFulIdx != ios->sEmpIdx ) {
			uart->ctlr |= TIEN_MASK;	// Turn on the transmit interrupt
		}
	}
}

void ionotifier_run() {
	int 	  ret;
	int 	  reply;
	int		  serverTid;
	char	  ch;
	IORequest req;

	// Initialize this notifier
	int event = ionotifier_init (&serverTid);
	
	FOREVER {
		ret = AwaitEvent(event, (char*) &ch, sizeof(ch)); 
		switch( ret ) {
			case SERIAL_OVERRUN: // Handle overrun error
				error( ret, "Serial IO data has been overrun." );
				break;
			case RECEIVE_NOT_EMPTY: // We just read a character
				req.type    = NOTIFY_GET;
				req.data[0] = ch;
				req.len     = 1;
				break;
			case MODEM_BIT_CHANGE:
			case TRANSMIT_NOT_FULL: // We could be clear to send
				req.type    = NOTIFY_PUT;
				req.data[0] = 0;
				req.len     = 0;
				break;
			default: // Handle all the other errors
				error( ret, "ionotifier: AwaitEvent failed" );
		}

		if ( ret >= NO_ERROR ) {
			ret = Send( serverTid, (char*) &req, sizeof(IORequest),
					     (char *) &reply, sizeof(reply) );
			// Handle errors
			if_error( ret, "ionotifier: Send failed" );
		}
	}

	Exit(); // This will never be called.
}

int ionotifier_init (int *serverTid) {
	debug ("ionotifier_init\r\n");
	int event;

	// Synchronize with the server
	Receive ( serverTid,  (char*) &event, sizeof(int) );
	Reply   ( *serverTid, (char*) &event, sizeof(int) );

	return event;
}	
