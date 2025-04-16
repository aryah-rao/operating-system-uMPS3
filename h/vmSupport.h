#ifndef VMSUPPORT_H
#define VMSUPPORT_H

/******************************* vmSupport.h *******************************
 *
 * This header file contains declarations for the virtual memory support
 * functions.
 *
 * Written by Aryah Rao and Anish Reddy
 *
 ****************************************************************************/

/* Included Header Files */
#include "../h/initial.h"
#include "../h/sysSupport.h"
#include "../h/initProc.h"

/* Function Declarations */
extern void             initSupportStructFreeList();            /* Initialize the Support Structure free list */
extern support_PTR      allocateSupportStruct();                /* Allocate a Support Structure from the free list */
extern void             initSwapPool();                         /* Initialize all swap pool data structures */
extern void             pager();                                /* Pager function for handling page faults */
extern void             uTLB_RefillHandler();                   /* TLB refill handler */
extern void             terminateUProcess(int *mutex);          /* Terminate the current user process */
extern void             setInterrupts(int toggle);              /* Set interrupts on or off */
extern void             resumeState(state_t *state);            /* Load processor state */
extern int              validateUserAddress(memaddr address);   /* Check if an address is in user space */

#endif /* VMSUPPORT_H */