/* filepath: /Volumes/Users/rao_a1/Documents/CS 372/pandos/h/deviceSupportDMA.h */
#ifndef DEVICESUPPORTDMA_H
#define DEVICESUPPORTDMA_H

/******************************* deviceSupportDMA.h ****************************
 *
 * Module: DMA Device Support Header
 *
 * Description:
 * This header file contains the function prototypes for the DMA device
 * support module (deviceSupportDMA.c). It declares the handlers for
 * disk and flash device system calls (SYS14-SYS17) and the raw flash
 * read/write function used internally and by the pager.
 *
 * Written by Aryah Rao & Anish Reddy
 *
 *****************************************************************************/

/* Included Header Files */
#include "initProc.h"

/*----------------------------------------------------------------------------*/
/* Function Prototypes */
/*----------------------------------------------------------------------------*/

/* Raw Flash Read/Write (Used by syscall handlers and pager) */
extern int flashRW(int operation, int flashNum, int blockNum, memaddr bufferAddr);

/* Syscall Handlers (Called by sysSupport.c) */
extern int diskPutSyscallHandler(support_PTR supportStruct);
extern int diskGetSyscallHandler(support_PTR supportStruct);
extern int flashPutSyscallHandler(support_PTR supportStruct);
extern int flashGetSyscallHandler(support_PTR supportStruct);

#endif /* DEVICESUPPORTDMA_H */