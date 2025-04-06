#ifndef VMSUPPORT_H
#define VMSUPPORT_H

/******************************* vmSupport.h *******************************
 *
 * 
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
extern void uTLB_RefillHandler();
extern void initSwapPool();                     /* Initialize the Swap Pool and semaphore */

/* Utility Functions */
extern void setInterrupts(int onOff);     /* Set interrupts on or off */
extern void loadState(state_t *state);    /* Load processor state */
extern void terminateUProc();               /* Terminate the current user process */

/***************************************************************/

#endif /* VMSUPPORT_H */