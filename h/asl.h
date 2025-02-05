#ifndef ASL
#define ASL

/************************** ASL.H ******************************
*
*  The externals declaration file for the Active Semaphore List
*    Module.
*
*  Written by Mikeyg
*
*  Modified by Aryah Rao and Anish Reddy
*
***************************************************************/

#include "../h/types.h"

extern int          insertBlocked (int *semAdd, pcb_PTR p);    /* Insert a PCB into the ASL */
extern pcb_PTR        removeBlocked (int *semAdd);               /* Remove the first PCB from the ASL */
extern pcb_PTR        outBlocked (pcb_PTR p);                   /* Remove specific PCB from the ASL */
extern pcb_PTR        headBlocked (int *semAdd);                 /* Get the head of the process queue of a semaphore */
extern void         initASL ();                                 /* Initialize the ASL */

/***************************************************************/

#endif