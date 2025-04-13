#ifndef VMSUPPORT_H
#define VMSUPPORT_H

/******************************* vmSupport.h *******************************
 *
 * This header file contains declarations for the virtual memory support
 * functions. It supports the TLB miss handler, pager, and swap pool operations.
 *
 * Written by Aryah Rao and Anish Reddy
 *
 ****************************************************************************/

/* Included Header Files */
#include "../h/initial.h"
#include "../h/sysSupport.h"
#include "../h/initProc.h"

/* Function Declarations */
extern void pager();                          /* Pager function for handling page faults */
extern void uTLB_RefillHandler();             /* TLB refill handler for fast TLB misses */
extern void initSwapPool();                   /* Initialize the Swap Pool and semaphore */
extern void clearSwapPoolEntries(int asid);   /* Clear swap pool entries for a process */

/* Support Structure Management */
extern void initSupportStructFreeList();      /* Initialize the Support Structure free list */
extern support_PTR allocSupportStruct();      /* Allocate a Support Structure from the free list */
extern void freeSupportStruct(support_PTR supportStruct); /* Return a Support Structure to the free list */

/* Utility Functions */
extern void setInterrupts(int onOff);         /* Set interrupts on or off */
extern void resumeState(state_t *state);      /* Load processor state */
extern void terminateUProc(int *mutex);       /* Terminate the current user process */
extern int validateUserAddress(unsigned int address); /* Validate user address */

/***************************************************************/

#endif /* VMSUPPORT_H */