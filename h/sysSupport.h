#ifndef SYSSUPPORT_H
#define SYSSUPPORT_H

/******************************* sysSupport.h *******************************
 *
 * This header file contains declarations for the Support Level's exception
 * handlers and related functions.
 * 
 * Written by Aryah Rao and Anish Reddy
 *
 ****************************************************************************/

/* Included Header Files */
#include "../h/initial.h"
#include "../h/vmSupport.h"
#include "../h/initProc.h"
#include "../h/deviceSupportDMA.h"

/* Function Declarations */
extern void             genExceptionHandler();                                  /* General exception handler for support level */
extern void             syscallExceptionHandler(support_PTR supportStruct);     /* SYSCALL exception handler */
extern support_PTR      getCurrentSupportStruct();                              /* Get current process's support structure */

/***************************************************************/

#endif /* SYSSUPPORT_H */