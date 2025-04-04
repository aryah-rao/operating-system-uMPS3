#ifndef SYSSUPPORT_H
#define SYSSUPPORT_H

/******************************* sysSupport.h *******************************
 *
 * This header file contains declarations for the Support Level's exception handlers
 * and utility functions for system-level operations.
 *
 * Written by Aryah Rao and Anish Reddy
 *
 ****************************************************************************/

/* Included Header Files */
#include "../h/initial.h"
#include "../h/vmSupport.h"
#include "../h/initProc.h"

/* Function Declarations */
extern void genExceptionHandler();        /* General exception handler */
extern void syscallExceptionHandler();    /* SYSCALL exception handler */
extern void programTrapExceptionHandler();/* Program trap exception handler */

/* Utility Functions */
extern void setInterrupts(int onOff);     /* Set interrupts on or off */
extern void loadState(state_t *state);    /* Load processor state */

/***************************************************************/

#endif /* SYSSUPPORT_H */