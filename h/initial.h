#ifndef INITIAL_H
#define INITIAL_H

/************************** INITIAL.H ******************************
 *
 *  The externals declaration file for the initial.c Module.
 *
 *  Written by Aryah Rao and Anish Reddy
 *
 *
 ***************************************************************/

#include "/usr/include/umps3/umps/libumps.h"
#include "../h/asl.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"

extern int processCount;
extern int softBlockCount;
extern pcb_PTR readyQueue;
extern pcb_PTR currentProcess;

extern void uTLB_RefillHandler();
extern void fooBar();

/***************************************************************/

#endif