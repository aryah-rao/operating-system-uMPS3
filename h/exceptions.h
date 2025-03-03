#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

/************************** EXCEPTIONS.H ******************************
 *
 *  The externals declaration file for the exceptions.c Module.
 *
 *  Written by Aryah Rao and Anish Reddy
 *
 *
 ***************************************************************/

#include "/usr/include/umps3/umps/libumps.h"
#include "../h/const.h"
#include "../h/types.h"
#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/scheduler.h"
#include "../h/initial.h"
#include "../h/interrupts.h"

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
extern void         uTLB_RefillHandler();                               /* TLB-Refill event handler */
extern void         tlbExceptionHandler();                              /* TLB exception handler */
extern void         programTrapHandler();                               /* Program Trap handler */
extern void         passUpOrDie(int exceptionType);                     /* Pass up or die */
extern void         copyState(state_t *dest, state_t *src);             /* Copy state */
extern int          updateCurrentProcess(state_t *exceptionState);      /* Update current process */

/***************************************************************/

#endif