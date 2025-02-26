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

extern void         exceptionHandler();         /* Exception handler */
extern void         uTLB_RefillHandler();       /* TLB-Refill event handler */
extern void         passUpOrDie(int exceptionType, state_t *exceptionState); /* Pass up or die */
extern void         getCpuTime(state_t *exceptionState); /* Get CPU time */
extern void         waitIO(state_t *exceptionState); /* Wait for IO */
extern void         verhogen(state_t *exceptionState); /* V operation */
extern void         passeren(state_t *exceptionState); /* P operation */
extern void         tlbExceptionHandler(state_t *exceptionState); /* TLB exception handler */
extern void         programTrapHandler(state_t *exceptionState); /* Program Trap handler */
extern void         syscallHandler(state_t *exceptionState); /* Syscall handler */

/***************************************************************/

#endif