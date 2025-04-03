#ifndef INITPROC_H
#define INITPROC_H

/******************************* initProc.h *******************************
 *
 * 
 *
 * Written by Aryah Rao and Anish Reddy
 *
 ****************************************************************************/

/* Included Header Files */
#include "../h/initial.h"
#include "../h/sysSupport.h"
#include "../h/vmSupport.h"

/* External Variables */
extern int masterSema4;                            /* Master semaphore for synchronization */
extern int swapPoolSema4;                          /* Semaphore for Swap Pool access */
extern int deviceMutex[DEVICE_COUNT];         /* Semaphores for device synchronization */
extern support_t supportStructures[UPROCMAX+1]; /* Static array of Support Structures - index 0 is reserved/sentinel */

/* Function Declarations */

/***************************************************************/

#endif /* INITPROC_H */