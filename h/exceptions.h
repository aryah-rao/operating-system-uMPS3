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

#include "../h/initial.h"

extern void         exceptionHandler();         /* Exception handler */
extern void         uTLB_RefillHandler();       /* TLB-Refill event handler */
extern void         syscallHandler();           /* */

/***************************************************************/

#endif