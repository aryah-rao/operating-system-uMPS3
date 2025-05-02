/* filepath: /Volumes/Users/rao_a1/Documents/CS 372/pandos/phase4/deviceSupportDMA.c */
/******************************* deviceSupportDMA.c ******************************
 *
 * Module: DMA Device Support
 *
 * Description:
 * This module implements support for block-based DMA devices (disk and flash)
 * and provides handlers for SYS14–SYS17 system calls. It manages data transfer
 * between user space and device DMA buffers.
 *
 * Policy Decisions:
 * - DMA Buffering: Dedicated kernel DMA buffers are used for all disk/flash
 *   operations initiated via syscalls to ensure proper physical memory alignment
 *   and prevent direct user access to device registers.
 * - Backing Store Protection: Access to flash device blocks 0-31 (reserved for
 *   backing store) via syscalls is prohibited and results in process termination.
 * - Parameter Validation: User-provided addresses and device/sector/block numbers
 *   are validated; invalid parameters lead to process termination.
 *
 * Functions:
 * - flashRW: Performs read/write to flash device
 * - diskPutSyscallHandler: Implements SYS14 (DISK_PUT)
 * - diskGetSyscallHandler: Implements SYS15 (DISK_GET)
 * - flashPutSyscallHandler: Implements SYS16 (FLASH_PUT)
 * - flashGetSyscallHandler: Implements SYS17 (FLASH_GET)
 * - copyBlock: Helper for copying data between memory and device buffers
 *
 * Written by Aryah Rao & Anish Reddy
 *
 *****************************************************************************/

#include "../h/deviceSupportDMA.h"

/*----------------------------------------------------------------------------*/
/* Helper Function Declarations */
/*----------------------------------------------------------------------------*/

HIDDEN void copyBlock(memaddr *src, memaddr *dest);

/*----------------------------------------------------------------------------*/
/* Global Function Implementations */
/*----------------------------------------------------------------------------*/

/******************************************************************************
 * Function: flashRW
 *
 * Description: Performs a read or write operation on a flash device using
 *              the provided physical address. This function is intended for use 
 *              by syscall handlers (which provide a DMA buffer address) and the 
 *              pager (which provides a physical frame address)
 *
 * Parameters:
 *              operation - READ (2) or WRITE (3)
 *              flashNum - Flash device number (0-7)
 *              blockNum - Block number on the flash device
 *              address - The physical address to use for the device's d_data0
 *
 * Returns:
 *              READY (1) on successful completion
 *              Negative value of the device status code on error
 *
 *****************************************************************************/
int flashRW(int operation, int flashNum, int blockNum, memaddr address) {
    /* Access device registers */
    devregarea_t *devRegisterArea = (devregarea_t *)RAMBASEADDR;
    int devIndex = ((FLASHINT - MAPINT) * DEV_PER_LINE) + (flashNum);

    /* Gain device mutex */
    SYSCALL(PASSEREN, (int)&deviceMutex[devIndex], 0, 0);

    /* Perform atomic device operation */
    setInterrupts(OFF);
    devRegisterArea->devreg[devIndex].d_data0 = address;
    devRegisterArea->devreg[devIndex].d_command = (blockNum << FLASHSHIFT) | operation;
    int status = SYSCALL(WAITIO, FLASHINT, flashNum, FALSE);
    setInterrupts(ON);

    /* Release device mutex */
    SYSCALL(VERHOGEN, (int)&deviceMutex[devIndex], 0, 0);

    /* Return status */
    if (status != READY) {
        status = -status; 
    }
    return status;
}


/******************************************************************************
 * Function: diskPutSyscallHandler
 *
 * Description: Handles the SYS14 (DISK_PUT) system call. Writes a page of data
 *              from the user-provided logical address to the specified disk
 *              block using a dedicated kernel DMA buffer
 *
 * Parameters:
 *              supportStruct - Pointer to the current process's support structure
 *
 * Returns:
 *              READY (1) on successful completion
 *              Negative value of the device status code on error
 *              ERROR if parameters are invalid (terminates the process)
 *
 *****************************************************************************/
int diskPutSyscallHandler(support_PTR supportStruct) {
    /* Extract parameters from the exception state */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
    memaddr logicalAddress = exceptState->s_a1;
    int diskNum = exceptState->s_a2;
    int linearSector = exceptState->s_a3;

    /* Access device registers */
    devregarea_t *devRegisterArea = (devregarea_t *)RAMBASEADDR;
    int devIndex = ((DISKINT - MAPINT) * DEV_PER_LINE) + (diskNum);

    /* Validate basic parameters */
    if (diskNum <= 0 || diskNum >= DEV_PER_LINE || linearSector < 0 || !validateUserAddress(logicalAddress)) {
        terminateUProcess(NULL);
        return ERROR;
    }

    /* Get DMA buffer address */
    memaddr diskDmaBufferAddr = DISK_DMABUFFER_ADDR(diskNum);

    /* Gain device mutex */
    SYSCALL(PASSEREN, (int)&deviceMutex[devIndex], 0, 0);

    /* Read disk parameters from d_data1 */
    unsigned int disk_data1 = devRegisterArea->devreg[devIndex].d_data1;
    unsigned int max_sect = (disk_data1 & DISKSECTORMASK);          /* Extract bits 0-7 */
    unsigned int max_head = (disk_data1 & DISKHEADRMASK) >> 8;     /* Extract bits 8-15 */
    unsigned int max_cyl  = (disk_data1 & DISKCYLINDERRMASK) >> 16;    /* Extract bits 16-31 */
    unsigned int sectors_per_cyl = max_head * max_sect;         /* Sectors per cylinder */
    unsigned int total_sectors = max_cyl * sectors_per_cyl;     /* Total number of sectors */

    /* Check for invalid disk parameters */
    if (max_sect == 0 || max_head == 0 || max_cyl == 0 || linearSector >= total_sectors) {
        terminateUProcess(&deviceMutex[devIndex]);
        return ERROR;
    }

    /* Calculate target cylinder, head, sector */
    unsigned int cyl = linearSector / sectors_per_cyl;
    unsigned int head = (linearSector % sectors_per_cyl) / max_sect;
    unsigned int sect = (linearSector % sectors_per_cyl) % max_sect;

    /* Perform SEEKCYL operation atomically */
    setInterrupts(OFF);
    devRegisterArea->devreg[devIndex].d_command = (cyl << 8) | SEEKCYL;
    int status = SYSCALL(WAITIO, DISKINT, diskNum, FALSE);
    setInterrupts(ON);

    if (status != READY) { /* If SEEKCYL failed */
        SYSCALL(VERHOGEN, (int)&deviceMutex[devIndex], 0, 0); /* Release device mutex */
        return -status;
    }

    /* Copy data from user logical address to kernel DMA buffer */
    copyBlock((memaddr *)logicalAddress, (memaddr *)diskDmaBufferAddr);

    /* Perform WRITEBLK operation atomically */
    setInterrupts(OFF);
    devRegisterArea->devreg[devIndex].d_data0 = diskDmaBufferAddr;
    devRegisterArea->devreg[devIndex].d_command = (head << 16) | (sect << 8) | WRITEBLK;
    status = SYSCALL(WAITIO, DISKINT, diskNum, FALSE);
    setInterrupts(ON);

    /* Release device mutex */
    SYSCALL(VERHOGEN, (int)&deviceMutex[devIndex], 0, 0);

    /* Return status */
    if (status != READY) {
        status = -status;
    }
    return status;
}


/******************************************************************************
 * Function: diskGetSyscallHandler
 *
 * Description: Handles the SYS15 (DISK_GET) system call. Reads a page of data
 *              from the specified disk block into a dedicated kernel DMA buffer,
 *              then copies it to the user-provided logical address
 *
 * Parameters:
 *              supportStruct - Pointer to the current process's support structure
 *
 * Returns:
 *              READY (1) on successful completion
 *              Negative value of the device status code on error
 *              ERROR if parameters are invalid (terminates the process)
 * 
 *****************************************************************************/
int diskGetSyscallHandler(support_PTR supportStruct) {
    /* Extract parameters from the exception state */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
    memaddr logicalAddress = exceptState->s_a1;
    int diskNum = exceptState->s_a2;
    int linearSector = exceptState->s_a3;

    /* Access device registers */
    devregarea_t *devRegisterArea = (devregarea_t *)RAMBASEADDR;
    int devIndex = ((DISKINT - MAPINT) * DEV_PER_LINE) + (diskNum);

    /* Validate basic parameters */
    if (diskNum <= 0 || diskNum >= DEV_PER_LINE || linearSector < 0 || !validateUserAddress(logicalAddress)) {
        terminateUProcess(NULL);
        return ERROR;
    }

    /* Get DMA buffer address */
    memaddr diskDmaBufferAddr = DISK_DMABUFFER_ADDR(diskNum);

    /* Gain device mutex */
    SYSCALL(PASSEREN, (int)&deviceMutex[devIndex], 0, 0);

    /* Read disk parameters from d_data1 */
    unsigned int disk_data1 = devRegisterArea->devreg[devIndex].d_data1;
    unsigned int max_sect = (disk_data1 & 0x000000FF);          /* Extract bits 0-7 */
    unsigned int max_head = (disk_data1 & 0x0000FF00) >> 8;     /* Extract bits 8-15 */
    unsigned int max_cyl  = (disk_data1 & 0xFFFF0000) >> 16;    /* Extract bits 16-31 */
    unsigned int sectors_per_cyl = max_head * max_sect;         /* Sectors per cylinder */
    unsigned int total_sectors = max_cyl * sectors_per_cyl;     /* Total number of sectors */

    /* Check for invalid disk parameters */
    if (max_sect == 0 || max_head == 0 || max_cyl == 0 || linearSector >= total_sectors) {
        terminateUProcess(&deviceMutex[devIndex]);
        return ERROR;
    }

    /* Calculate target cylinder, head, sector */
    unsigned int cyl = linearSector / sectors_per_cyl;
    unsigned int temp = linearSector % sectors_per_cyl;
    unsigned int head = temp / max_sect;
    unsigned int sect = temp % max_sect;

    /* Perform SEEKCYL operation atomically */
    setInterrupts(OFF);
    devRegisterArea->devreg[devIndex].d_command = (cyl << 8) | SEEKCYL;
    int status = SYSCALL(WAITIO, DISKINT, diskNum, FALSE);
    setInterrupts(ON);

    if (status != READY) {
        SYSCALL(VERHOGEN, (int)&deviceMutex[devIndex], 0, 0); /* Release mutex */
        return -status;
    }

    /* Perform READBLK operation */
    setInterrupts(OFF);
    devRegisterArea->devreg[devIndex].d_data0 = diskDmaBufferAddr;
    devRegisterArea->devreg[devIndex].d_command = (head << 16) | (sect << 8) | READBLK;
    status = SYSCALL(WAITIO, DISKINT, diskNum, FALSE);
    setInterrupts(ON);

    if (status != READY) {  /* If READBLK failed */
        return -status;
    }

    /* Copy data from kernel DMA buffer to user logical address */
    copyBlock((memaddr *)diskDmaBufferAddr, (memaddr *)logicalAddress);

    /* Release device mutex */
    SYSCALL(VERHOGEN, (int)&deviceMutex[devIndex], 0, 0);

    return status; /* Return status */
}

/******************************************************************************
 * Function: flashPutSyscallHandler
 *
 * Description: Handles the SYS16 (FLASH_PUT) system call. Writes a page of data
 *              from the user-provided logical address to the specified flash
 *              block (>= 32) using a dedicated kernel DMA buffer
 *
 * Parameters:
 *              supportStruct - Pointer to the current process's support structure
 *
 * Returns:
 *              READY (1) on successful completion
 *              Negative value of the device status code on error
 *              ERROR if parameters are invalid (terminates the process)
 *
 *****************************************************************************/
int flashPutSyscallHandler(support_PTR supportStruct) {
    /* Extract parameters from the exception state */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
    memaddr logicalAddress = exceptState->s_a1;
    int flashNum = exceptState->s_a2;
    int blockNum = exceptState->s_a3;

    /* Validate parameters */
    if (flashNum < 0 || flashNum >= DEV_PER_LINE || blockNum < 32 || !validateUserAddress(logicalAddress)) {
        terminateUProcess(NULL);
        return ERROR;
    }

    /* Get DMA buffer address */
    memaddr flashDmaBufferAddr = FLASH_DMABUFFER_ADDR(flashNum);

    /* Copy data from user logical address to kernel DMA buffer */
    copyBlock((memaddr *)logicalAddress, (memaddr *)flashDmaBufferAddr);

    /* Call flashRW using the DMA buffer address */
    int status = flashRW(WRITE, flashNum, blockNum, flashDmaBufferAddr);

    return status; /* Return status */
}

/******************************************************************************
 * Function: flashGetSyscallHandler
 *
 * Description: Handles the SYS17 (FLASH_GET) system call. Reads a page of data
 *              from the specified flash block (>= 32) into a dedicated kernel
 *              DMA buffer, then copies it to the user-provided logical address
 *
 * Parameters:
 *              supportStruct - Pointer to the current process's support structure
 *
 * Returns:
 *              READY (1) on successful completion
 *              Negative value of the device status code on error
 *              ERROR if parameters are invalid (terminates the process)
 *
 *****************************************************************************/
int flashGetSyscallHandler(support_PTR supportStruct) {
    /* Extract parameters from the exception state */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
    memaddr logicalAddress = exceptState->s_a1;
    int flashNum = exceptState->s_a2;
    int blockNum = exceptState->s_a3;

    /* Validate parameters */
    if (flashNum < 0 || flashNum >= DEV_PER_LINE || blockNum < 32 || !validateUserAddress(logicalAddress)) {
        terminateUProcess(NULL);
        return ERROR;
    }

    /* Get DMA buffer address */
    memaddr flashDmaBufferAddr = FLASH_DMABUFFER_ADDR(flashNum);

    /* Call flashRW using the DMA buffer address */
    int status = flashRW(READ, flashNum, blockNum, flashDmaBufferAddr);

    /* If read was successful, copy data from DMA buffer to user logical address */
    if (status != READY) {
        return -status;
    }

    /* Copy data from kernel DMA buffer to user logical address */
    copyBlock((memaddr *)flashDmaBufferAddr, (memaddr *)logicalAddress);

    return status;  /* Return status */
}

/*----------------------------------------------------------------------------*/
/* Helper Function Implementations */
/*----------------------------------------------------------------------------*/

/******************************************************************************
 * Function: copyBlock
 *
 * Description: Copies PAGESIZE (4096) bytes word by word from source to destination
 *
 * Parameters:
 *              src - Pointer to the source buffer
 *              dest - Pointer to the destination buffer
 *
 * Returns:
 *              None
 *****************************************************************************/
void copyBlock(memaddr *src, memaddr *dest) {
    /* Copy word by word */
    int word;
    for (word = 0; word < (PAGESIZE / WORDLEN); word++) {
        *dest = *src;
        dest++;
        src++;
    }
}