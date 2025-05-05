#ifndef DEVICESUPPORTDMA_H
#define DEVICESUPPORTDMA_H

/******************************* deviceSupportDMA.h ****************************
 *
 * Module: DMA Device Support Header
 *
 * Description:
 * This header file contains the function prototypes for the DMA device
 * support module.
 *
 * Written by Aryah Rao & Anish Reddy
 *
 *****************************************************************************/

/* Included Header Files */
#include "initProc.h"

/* Function Declarations */
extern int              diskRW(int operation, int diskNum, int sector, memaddr bufferAddr);     /* Perform read/write on disk */
extern int              flashRW(int operation, int flashNum, int blockNum, memaddr bufferAddr); /* Perform read/write on flash */
extern int              diskPutSyscallHandler(support_PTR supportStruct);                       /* Handles SYS14 (DISK_PUT) */
extern int              diskGetSyscallHandler(support_PTR supportStruct);                       /* Handles SYS15 (DISK_GET) */
extern int              flashPutSyscallHandler(support_PTR supportStruct);                      /* Handles SYS16 (FLASH_PUT) */
extern int              flashGetSyscallHandler(support_PTR supportStruct);                      /* Handles SYS17 (FLASH_GET) */

#endif /* DEVICESUPPORTDMA_H */