/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 */
#define DEBUG 1

#include <ts7200.h>
#include <buffer.h>

#include "debug.h"
#include "drivers.h"
#include "error.h"
#include "requests.h"
#include "servers.h"

#define AWAITBUF_LEN	1

//-----------------------------------------------------------------------------
// Private members & Forward Declarations

typedef struct {
	char sendBuffer[NUM_ENTRIES];
	char recvBuffer[NUM_ENTRIES];
	int waitBuffer[NUM_ENTRIES];	// Array of waiting tids (for a char)

	RB	send;
	RB	recv;
	RB	wait;
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

void ios_run (UART *uart) {
	debug ("ios_run: The Serial IO Server is about to start. \r\n");	
	
	int				senderTid;		// The task id of the message sender
	IORequest		req;			// A serial io server request message
	int				len;
	int				i;
	SerialIOServer 	ios;
	char 			ch;

	// Initialize the Serial IO Server
	ios_init (&ios, uart);

	FOREVER {
		// Receive a server request
		len = Receive ( &senderTid, (char *) &req, sizeof(IORequest) );
		
		assert( len >= IO_REQUEST_SIZE + req.len );
		assert( senderTid >= 0 );
		
		// Handle the request
		switch (req.type) {
			case NOTIFY_GET:
				debug ("ios: notified GET data=%c \r\n", req.data[0] );
				
				// Reply to the Notifier immediately
				Reply (senderTid, (char*) &req.data, sizeof(char));
				
				// store the character on the buffer
				debug("ios: notified is storing ch=%c\r\n", req.data[0]);
				assert( rb_empty( &ios.wait ) || rb_empty( &ios.recv ) );
				rb_push( &ios.recv, &req.data[0] );

				ch = req.data[0];
				// If we have a character and someone is blocked, wake them up
				while ( !rb_empty( &ios.wait ) ) {
					senderTid = *((int *) rb_pop( &ios.wait ));
					debug ("ios: waking %d to give ch=%c\r\n", senderTid, ch);
					// Keep replying until it works
					if(Reply( senderTid, &ch, sizeof(char)) >= NO_ERROR) {
						rb_pop( &ios.recv );
						break;
					}
				}
				
				break;

			case NOTIFY_PUT:
				debug ("ios: notified PUT \r\n");
				
				// Reply to the Notifier immediately
				Reply (senderTid, (char*) &req.data, sizeof(char));
				
				// Try to send the data across the UART!
				ios_attemptTransmit( &ios, uart );
				break;
			
			case GETC:
				// If we have a character, send it back!
				if ( !rb_empty( &ios.recv ) ) {
					ch = *( (char *) rb_pop( &ios.recv ) );
					debug ("ios: getc has a char=%c\r\n", ch);
					// Make sure no one else was waiting first ...
					assert ( rb_empty( &ios.wait) );	

					Reply (senderTid, (char *)&ch,  sizeof(char));
					
				// Otherwise, store the tid until we get a character
				} else {
					debug ("ios: getc, but NO char \r\n" );

					rb_push( &ios.wait, &senderTid );
				}

				break;

			case PURGE:
				// We need to purge the receive in buffer
				rb_init( &ios.recv, ios.recvBuffer );	
				Reply (senderTid, 0, 0);
				break;
			
			case PUTC:
			case PUTSTR:
				debug ("ios: putstr str=%s len=%d\r\n", req.data, req.len);
				// Do not let the sender stay blocked for long.
				Reply (senderTid, 0, 0);

				// Copy the data to our send buffer
				for ( i = 0; i < req.len; i ++ ) {
					rb_push( &ios.send, &req.data[i] );
				}

				// Try to send some data across the UART
				ios_attemptTransmit ( &ios, uart );

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
	int tid;
	char c;
	int notifierEvt = (uart == UART1) ? INT_UART1 : INT_UART2;

	// Create the notifier
	if ( (tid = Create (1, &ionotifier_run)) < NO_ERROR) {
		error (tid, "Cannot make IO notifier.\r\n");
	}
	
	// Send/Receive to synchronize with the notifier
	// This tells the notifier the server's tid & event to await
	Send ( tid, (char*) &notifierEvt, sizeof(int), 0, 0);
	
	// Initialize the buffers
	rb_init( &s->send, s->sendBuffer );
	rb_init( &s->recv, s->recvBuffer );
	rb_init( &s->wait, s->waitBuffer );

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
	char ch;
	debug( "ios: attempting to transmit\r\n" );

	if ( !rb_empty( &ios->send ) ) {
		
		// If the transmitter is busy, wait for interrupt
		if ( uart->flag & TXBUSY_MASK ) {
			debug ( "ios: transmit busy. Turning on transmit interrupt.\r\n");
			uart->ctlr |= TIEN_MASK;	// Turn on the transmit interrupt
		
		// Otherwise, write a single byte
		} else if( (uart->flag & CTS_MASK) || (uart != UART1) ) {	
			// Get the character to send
			ch = *( (char *) rb_pop( &ios->send ) );
			debug ("ios: writing '%c' to uart\r\n", ch);

			uart_write ( uart, ch );

			// If we want to send more, enable the interrupt
			if ( !rb_empty( &ios->send ) ) {
				uart->ctlr |= TIEN_MASK;	// Turn on the transmit interrupt
			}
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
	Reply   ( *serverTid, 0, 0 );

	return event;
}	
