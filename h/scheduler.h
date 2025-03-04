#ifndef SCHEDULER_H
#define SCHEDULER_H

/******************************* scheduler.h *********************************
 *
 * This header file contains the declarations for the scheduler functions.
 * It establishes the interface for the scheduler.c module.
 * 
 * Written by Aryah Rao and Anish Reddy
 * 
 ****************************************************************************/

/* Included Header Files */
#include "/usr/include/umps3/umps/libumps.h"
#include "../h/asl.h"
#include "../h/initial.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"

/* Function Declarations */
extern void         scheduler();                                                /* Scheduler */ 
extern void         loadProcessState(state_t *state, unsigned int quantum);     /* Load process state */
extern pcb_PTR      getProcess(pcb_PTR process);                                /* Get process */

#endif /* SCHEDULER_H */