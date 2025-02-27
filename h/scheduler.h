#ifndef SCHEDULER_H
#define SCHEDULER_H

/************************** SCHEDULER.H ******************************
 *
 *  The externals declaration file for the scheduler.c Module.
 *
 *  Written by Aryah Rao and Anish Reddy
 *
 *
 ***************************************************************/

#include "../h/initial.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"

extern void         scheduler();
extern void         loadProcessState(state_t *state, unsigned int quantum);

/***************************************************************/

#endif