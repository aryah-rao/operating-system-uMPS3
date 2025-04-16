#ifndef INITPROC_H
#define INITPROC_H

/******************************* initProc.h *******************************
 *
 * This header file contains declarations for the test process.
 *
 * Written by Aryah Rao and Anish Reddy
 *
 ****************************************************************************/

/* Included Header Files */
#include "../h/initial.h"
#include "../h/sysSupport.h"
#include "../h/vmSupport.h"

/* External Variables */
extern int masterSema4;                     /* Master semaphore for synchronization */
extern int deviceMutex[DEVICE_COUNT];       /* Semaphores for device mutual exclusion */

/* Function Declarations */
extern void             test();             /* Test function to be called by the initital.c */

#endif /* INITPROC_H */