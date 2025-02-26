#ifndef INITIAL_H
#define INITIAL_H

/************************** INITIAL.H ******************************
 *
 *  The externals declaration file for the initial.c Module.
 *
 *  Written by Aryah Rao and Anish Reddy
 *
 *
 ***************************************************************/

#include "/usr/include/umps3/umps/libumps.h"
#include "../h/asl.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"

extern int processCount;                    /* Number of processes in system */
extern int softBlockCount;                  /* Number of blocked processes */
extern pcb_PTR readyQueue;                  /* Ready queue */
extern pcb_PTR currentProcess;              /* Currently executing process */
extern int deviceSemaphores[DEVICE_COUNT+1];  /* Array of device semaphores */
extern cpu_t startTOD;                      /* Time of day at system start */

extern void         test();

/***************************************************************/

#endif