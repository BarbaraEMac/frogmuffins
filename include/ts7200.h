/*
 * ts7200.h - definitions describing the ts7200 peripheral registers
 *
 * Specific to the TS-7200 ARM evaluation board
 *
 */
#ifndef __TS7200_H__
#define __TS7200_H__


#define	TIMER1_BASE	0x80810000
#define	TIMER2_BASE	0x80810020
#define	TIMER3_BASE	0x80810080

#define	LDR_OFFSET	0x00000000	// 16/32 bits, RW
#define	VAL_OFFSET	0x00000004	// 16/32 bits, RO
#define CRTL_OFFSET	0x00000008	// 3 bits, RW
	#define	ENABLE_MASK	0x00000080
	#define	MODE_MASK	0x00000040
	#define	CLKSEL_MASK	0x00000008
#define CLR_OFFSET	0x0000000c	// no data, WO


#define LED_ADDRESS	0x80840020
	#define LED_NONE	0x0
	#define LED_GREEN	0x1
	#define LED_RED		0x2
	#define LED_BOTH	0x3

#define COM1	0
#define COM2	1

#define IRDA_BASE	0x808b0000
#define UART1_BASE	0x808c0000
#define UART2_BASE	0x808d0000

// All the below registers for UART1
// First nine registers (up to Ox28) for UART 2

#define UART_DATA_OFFSET	0x0	// low 8 bits
	#define DATA_MASK	0xff
#define UART_RSR_OFFSET		0x4	// low 4 bits
	#define FE_MASK		0x1
	#define PE_MASK		0x2
	#define BE_MASK		0x4
	#define OE_MASK		0x8
#define UART_LCRH_OFFSET	0x8	// low 7 bits
	#define BRK_MASK	0x1
	#define PEN_MASK	0x2	// parity enable
	#define EPS_MASK	0x4	// even parity
	#define STP2_MASK	0x8	// 2 stop bits
	#define FEN_MASK	0x10	// fifo
	#define WLEN_MASK	0x60	// word length
#define UART_LCRM_OFFSET	0xc	// low 8 bits
	#define BRDH_MASK	0xff	// MSB of baud rate divisor
#define UART_LCRL_OFFSET	0x10	// low 8 bits
	#define BRDL_MASK	0xff	// LSB of baud rate divisor
#define UART_CTLR_OFFSET	0x14	// low 8 bits
	#define UARTEN_MASK	0x1
	#define MSIEN_MASK	0x8	// modem status int
	#define RIEN_MASK	0x10	// receive int
	#define TIEN_MASK	0x20	// transmit int
	#define RTIEN_MASK	0x40	// receive timeout int
	#define LBEN_MASK	0x80	// loopback 
#define UART_FLAG_OFFSET	0x18	// low 8 bits
	#define CTS_MASK	0x1
	#define DCD_MASK	0x2
	#define DSR_MASK	0x4
	#define TXBUSY_MASK	0x8
	#define RXFE_MASK	0x10	// Receive buffer empty
	#define TXFF_MASK	0x20	// Transmit buffer full
	#define RXFF_MASK	0x40	// Receive buffer full
	#define TXFE_MASK	0x80	// Transmit buffer empty
#define UART_INTR_OFFSET	0x1c
#define UART_DMAR_OFFSET	0x28

// Specific to UART1

#define UART_MDMCTL_OFFSET	0x100
#define UART_MDMSTS_OFFSET	0x104
#define UART_HDLCCTL_OFFSET	0x20c
#define UART_HDLCAMV_OFFSET	0x210
#define UART_HDLCAM_OFFSET	0x214
#define UART_HDLCRIB_OFFSET	0x218
#define UART_HDLCSTS_OFFSET	0x21c


// Specific to Vectored interrupts
#define VIC1_BASE			0x800B0000
#define VIC2_BASE			0x800C0000
#define VIC_RAW_INTR		0x8
#define VIC_INT_ENABLE		0x10
#define VIC_INT_EN_CLR	 	0x14
#define VIC_SOFT_INT		0x18
#define VIC_SOFT_INT_CLR 	0x1C

typedef volatile struct _uart {
	int data;
	int rsr;
	int lcrh;
	int lcrm;
	int lcrl;
	int ctlr;
	int flag;
	int intr;
} UART;

typedef enum _interrupt {
	// -		 =  0,	// Unused
	// -		 =  1,	// Unused
	COMMRX		 =  2,	// ARM Communication Rx for Debug
	COMMTX		 =  3,	// ARM Communication Tx for Debug
	TC1UI		 =  4,	// TC1 under flow interrupt (Timer Counter 1)
	TC2UI		 =  5,	// TC2 under flow interrupt (Timer Counter 2)
	AACINTR		 =  6,	// Advanced Audio Codec interrupt
	DMAM2P0		 =  7,	// DMA Memory to Peripheral Interrupt 0
	DMAM2P1		 =  8,	// DMA Memory to Peripheral Interrupt 1
	DMAM2P2		 =  9,	// DMA Memory to Peripheral Interrupt 2
	DMAM2P3		 = 10,	// DMA Memory to Peripheral Interrupt 3
	DMAM2P4		 = 11,	// DMA Memory to Peripheral Interrupt 4
	DMAM2P5		 = 12,	// DMA Memory to Peripheral Interrupt 5
	DMAM2P6		 = 13,	// DMA Memory to Peripheral Interrupt 6
	DMAM2P7		 = 14,	// DMA Memory to Peripheral Interrupt 7
	DMAM2P8		 = 15,	// DMA Memory to Peripheral Interrupt 8
	DMAM2P9		 = 16,	// DMA Memory to Peripheral Interrupt 9
	DMAM2M0		 = 17,	// DMA Memory to Memory Interrupt 0
	DMAM2M1		 = 18,	// DMA Memory to Memory Interrupt 1
	// -		 = 19,	// Reserved
	// -		 = 20,	// Reserved
	// -		 = 21,	// Reserved
	// -		 = 22,	// Reserved
	UART1RXINTR1 = 23,	// UART 1 Receive Interrupt
	UART1TXINTR1 = 24,	// UART 1 Transmit Interrupt
	UART2RXINTR2 = 25,	// UART 2 Receive Interrupt
	UART2TXINTR2 = 26,	// UART 2 Transmit Interrupt
	UART3RXINTR3 = 27,	// UART 3 Receive Interrupt
	UART3TXINTR3 = 28,	// UART 3 Transmit Interrupt
	INT_KEY		 = 29,	// Keyboard Matrix Interrupt
	NT_TOUCH	 = 30,	// Touch Screen Controller Interrupt
	INT_EXT0	 = 32,	// External Interrupt 0
	INT_EXT1	 = 33,	// External Interrupt 1
	INT_EXT2	 = 34,	// External Interrupt 2
	TINTR		 = 35,	// 64 Hz Tick Interrupt
	WEINT		 = 36,	// Watchdog Expired Interrupt
	INT_RTC		 = 37,	// RTC Interrupt
	INT_IrDA	 = 38,	// IrDA Interrupt
	INT_MAC		 = 39,	// Ethernet MAC Interrupt
	// -		 = 40,	// Reserved
	INT_PROG	 = 41,	// Raster Programmable Interrupt
	CLK1HZ		 = 42,	// 1 Hz Clock Interrupt
	V_SYNC		 = 43,	// Video Sync Interrupt
	INT_VIDEO_FIFO = 44,	// Raster Video FIFO Interrupt
	INT_SSP1RX	 = 45,	// SSP Receive Interrupt
	INT_SSP1TX	 = 46,	// SSP Transmit Interrupt
	// -		 = 47,	// Reserved
	// -		 = 48,	// Reserved
	// -		 = 49,	// Reserved
	// -		 = 50,	// Reserved
	TC3UI		 = 51,	// TC3 under flow interrupt (Timer Counter 3)
	INT_UART1	 = 52,	// UART 1 Interrupt
	SSPINTR		 = 53,	// Synchronous Serial Port Interrupt
	INT_UART2	 = 54,	// UART 2 Interrupt
	INT_UART3	 = 55,	// UART 3 Interrupt
	USHINTR		 = 56,	// USB Host Interrupt
	INT_PME		 = 57,	// Ethernet MAC PME Interrupt
	INT_DSP		 = 58,	// ARM Core Interrupt
	GPIOINTR	 = 59,	// GPIO Combined interrupt
	I2SINTR		 = 60	// I2S Block Combined interrupt
	// -		 = 61,	// Unused
	// -		 = 62,	// Unused
	// -		 = 63	// Unused

} Interrupt;

inline int readMemory(int addr) ;

inline void writeMemory(int addr, int value) ;

void intr_on( Interrupt eventId ) ;
void intr_off( Interrupt eventId ) ;
void intr_allOff() ;

int uart_setfifo( int channel, int state ) ;
int uart_setspeed( int channel, int speed ) ;

#endif
