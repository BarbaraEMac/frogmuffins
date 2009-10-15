/**
 * CS 452: Kernel
 * becmacdo
 * dgoc
 *
 * This file holds user task stuff.
 */

#ifndef __TASK_H__
#define __TASK_H__

//-------------------------------------------------------------------------
//---------------------------Kernel 3--------------------------------------
//-------------------------------------------------------------------------
void k3_firstUserTask();

void k3_client();
//-------------------------------------------------------------------------
//---------------------------Kernel 2--------------------------------------
//-------------------------------------------------------------------------
void k2_firstUserTask();

/**
 * The following 2 functions test the name server.
 * They spawn many tasks that RegisterAs and verify WhoIs is correct.
 */
void k2_registerTask1();
void k2_registerTask2();

/**
 * The following 2 functions verify that basic Send and Receive work.
 */
void k2_senderTask();
void k2_receiverTask();


//-------------------------------------------------------------------------
//---------------------------Kernel 1--------------------------------------
//-------------------------------------------------------------------------
void k1_firstUserTask();
void k1_userTaskStart();

#endif
