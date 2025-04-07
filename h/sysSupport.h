#ifndef SYSSUPPORT_H
#define SYSSUPPORT_H

/******************************* sysSupport.h *******************************
 *
 * This header file contains declarations for the Support Level's exception
 * handlers and related functions. It supports syscalls, program traps,
 * and other exception handling at the Support Level.
 *
 * Written by Aryah Rao and Anish Reddy
 *
 ****************************************************************************/

/* Included Header Files */
#include "../h/initial.h"
#include "../h/vmSupport.h"
#include "../h/initProc.h"

/* Function Declarations */
extern void genExceptionHandler();              /* General exception handler for support level */
extern void syscallExceptionHandler(support_PTR supportStruct); /* SYSCALL exception handler */
extern void programTrapExceptionHandler();     /* Program trap exception handler */
extern support_PTR getCurrentSupportStruct();  /* Get current process's support structure */

/* External declarations for global variables used by the support level */
extern int masterSema4;                        /* Master semaphore for synchronization */
extern int deviceMutex[DEVICE_COUNT];          /* Semaphores for device synchronization */

/***************************************************************/

#endif /* SYSSUPPORT_H */