#ifndef INTERRUPTS_H
#define INTERRUPTS_H

/******************************* interrupts.h *******************************
 *
 * This header file contains the declarations for the interrupt handling functions.
 * It establishes the interface for the interrupts.c module.
 *
 * Written by Aryah Rao and Anish Reddy
 *
 ****************************************************************************/

/* Included Header Files */
#include "/usr/include/umps3/umps/libumps.h"
#include "../h/initial.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"

/* Function Declarations */
extern void             interruptHandler();         /* Interrupt handler */

/***************************************************************/

#endif /* INTERRUPTS_H */