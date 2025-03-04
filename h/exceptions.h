#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

/******************************* exceptions.h *******************************
 *
 * This header file contains the declarations for the exception handling functions.
 * It establishes the interface for the exceptions.c module.
 *
 * Written by Aryah Rao and Anish Reddy
 *
 ****************************************************************************/

/* Included Header Files */
#include "/usr/include/umps3/umps/libumps.h"
#include "../h/const.h"
#include "../h/types.h"
#include "../h/types.h"
#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/scheduler.h"
#include "../h/initial.h"
#include "../h/initial.h"

/* Function Declarations */
extern void         exceptionHandler();                                 /* Exception handler */
extern void         syscallHandler();                                   /* Syscall handler */
extern void         createProcess();                                    /* Create process */
extern void         terminateProcess(pcb_PTR p);                        /* Terminate process */
extern void         passeren(int *semAdd);                              /* P operation */
extern pcb_PTR      verhogen();                                         /* V operation */
extern void         waitIO();                                           /* Wait for IO */
extern void         getCpuTime();                                       /* Get CPU time */
extern void         waitClock();                                        /* Wait for clock */
extern void         getSupportPtr();                                    /* Get support pointer */
extern void         tlbExceptionHandler();                              /* TLB exception handler */
extern void         programTrapHandler();                               /* Program Trap handler */
extern void         passUpOrDie(int exceptionType);                     /* Pass up or die */
extern void         copyState(state_t *dest, state_t *src);             /* Copy state */
extern int          updateCurrentProcess(state_t *exceptionState);      /* Update current process */
extern void         updateProcessTime();                                /* Update process time */

/***************************************************************/

#endif /* EXCEPTIONS_H */