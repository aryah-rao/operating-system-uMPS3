#ifndef DELAYDAEMON_H
#define DELAYDAEMON_H

/******************************* delayDaemon.h *******************************
 *
 * This header file contains declarations for the delay daemon and related
 * functions
 *
 * Written by Aryah Rao and Anish Reddy
 *
 ****************************************************************************/

/* Included Header Files */
#include "../h/initial.h"
#include "../h/sysSupport.h"
#include "../h/initProc.h"

/* Function Declarations */
extern void         delaySyscallHandler(support_PTR supportStruct);                              /* SYS18 handler */
extern void         initADL();                                          /* Initialize Delay Facility */

#endif /* DELAYDAEMON_H */