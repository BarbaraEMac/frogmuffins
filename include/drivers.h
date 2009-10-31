/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * This file contains hardware drivers so that the kernel can be portable and
 * does not need to know the specifics of interracting with the hardware when
 * an interrupt happens.
 */

#ifndef __DRIVERS_H__
#define __DRIVERS_H__

/**
 * The driver function for TIMER1 interrupts.
 * This must be installed by any user task that calls AwaitEvent(TIMER1)
 * retBuf - The buffer to return data to.
 * bufLen - The length of the return buffer.
 */
int timer1Driver (char *retBuf, int bufLen);

/**
 * The driver function for TIMER2 interrupts.
 * This must be installed by any user task that calls AwaitEvent(TIMER2)
 * retBuf - The buffer to return data to.
 * bufLen - The length of the return buffer.
 */
int timer2Driver (char *retBuf, int bufLen);

/*
 * Set up the UART
 * This should be called to install the driver and set up the UART
 */
int uart_install ( UART *uart, int speed, int fifo );

#endif
