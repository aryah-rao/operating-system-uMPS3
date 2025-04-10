#ifndef INITIAL_H
#define INITIAL_H

/******************************* initial.h *************************************
 *
 * This header file contains the declarations for the initialization functions.
 * It establishes the interface for the initial.c module.
 * 
 * Written by Aryah Rao and Anish Reddy
 * 
 ****************************************************************************/

/* Included Header Files */
#include "/usr/include/umps3/umps/libumps.h"
#include "../h/asl.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"
#include "../h/initProc.h"          /* For test() */

/* Global Variables */
extern int          processCount;                       /* Number of processes in system */
extern int          softBlockCount;                     /* Number of blocked processes */
extern pcb_PTR      readyQueueHigh;                     /* Ready queue (High Priority) */
extern pcb_PTR      readyQueueLow;                      /* Ready queue (Low Priority) */
extern pcb_PTR      currentProcess;                     /* Currently executing process */
extern int          deviceSemaphores[DEVICE_COUNT];     /* Array of device semaphores */
extern cpu_t        startTOD;                           /* Time of day at system start */

/* Function Declarations */
void                main();

#endif /* INITIAL_H */