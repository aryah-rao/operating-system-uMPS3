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

/***************************************************************/

#endif /* VMSUPPORT_H */