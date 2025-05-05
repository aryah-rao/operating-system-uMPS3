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
 * - Backing Store Protection: Access to flash device blocks 0-31 (reserved for
 *   backing store) via syscalls is prohibited and results in process termination.
 * - Parameter Validation: User-provided addresses and device/sector/block numbers
 *   are validated; invalid parameters lead to process termination.
 * - Mutex Management: The module assumes that the caller holds the appropriate
 *   device mutex before calling diskRW/flashRW.
 *
 * Functions:
 * - flashRW: Performs read/write to flash device
 * - diskRW: Performs read/write to disk device
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
 * Function: diskRW
 *
 * Description: Performs a read or write operation on a specified disk
 *              and sector using the provided physical address
 *              Handles geometry calculation, SEEK, and READ/WRITE commands
 *              Assumes the caller holds the appropriate device mutex.
 *
 * Parameters:
 *              operation - READBLK (3) or WRITEBLK (4)
 *              diskNum - Disk device number (0-7)
 *              sector - Linear sector number on the disk
 *              bufferAddr - Physical address of the buffer to read into/write from
 *
 * Returns:
 *              READY (1) on successful completion
 *              Negative value of the device status code on error
 *              ERROR (-1) for invalid disk parameters
 *
 *****************************************************************************/
int diskRW(int operation, int diskNum, int linearSector, memaddr bufferAddr) {
    /* Access device registers */
    devregarea_t *devRegisterArea = (devregarea_t *)RAMBASEADDR;
    int devIndex = ((DISKINT - MAPINT) * DEV_PER_LINE) + (diskNum);

    /* Read disk parameters from d_data1 */
    unsigned int disk_data1 = devRegisterArea->devreg[devIndex].d_data1;
    unsigned int max_sector = (disk_data1 & DISKSECTORMASK);
    unsigned int max_head = (disk_data1 & DISKHEADRMASK) >> DISK_DATA1_HEAD_SHIFT;
    unsigned int max_cylinder  = (disk_data1 & DISKCYLINDERRMASK) >> DISK_DATA1_CYL_SHIFT;
    unsigned int sectors_per_cylinder = max_head * max_sector;
    unsigned int total_sectors = max_cylinder * sectors_per_cylinder;

    /* Check for invalid disk parameters */
    if (linearSector >= total_sectors || max_sector == 0 || max_head == 0 || max_cylinder == 0) {
        return ERROR;
    }

    /* Calculate target cylinder, head, sector */
    unsigned int cylinder = linearSector / sectors_per_cylinder;
    unsigned int head = (linearSector % sectors_per_cylinder) / max_sector;
    unsigned int sector = (linearSector % sectors_per_cylinder) % max_sector;

    /* Perform SEEKCYL Operation atomically */
    setInterrupts(OFF);
    devRegisterArea->devreg[devIndex].d_command = (cylinder << DISK_SEEK_CYL_SHIFT) | SEEKCYL;
    int status = SYSCALL(WAITIO, DISKINT, diskNum, FALSE);
    setInterrupts(ON);
    if (status != READY) {
        return -status;
    }

    /* Perform READBLK/WRITEBLK Operation atomically */
    setInterrupts(OFF);
    devRegisterArea->devreg[devIndex].d_data0 = bufferAddr;
    devRegisterArea->devreg[devIndex].d_command = (head << DISK_COMMAND_HEAD_SHIFT) | (sector << DISK_COMMAND_SECT_SHIFT) | operation;
    status = SYSCALL(WAITIO, DISKINT, diskNum, FALSE);
    setInterrupts(ON);

    /* Return status */
    if (status != READY) {
        status = -status;
    }
    return status;
}


/******************************************************************************
 * Function: flashRW
 *
 * Description: Performs a read or write operation on a flash device using
 *              the provided physical address.
 *              **Assumes the caller holds the appropriate device mutex.**
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

    /* Perform atomic device operation */
    setInterrupts(OFF);
    devRegisterArea->devreg[devIndex].d_data0 = address;
    devRegisterArea->devreg[devIndex].d_command = (blockNum << FLASHSHIFT) | operation;
    int status = SYSCALL(WAITIO, FLASHINT, flashNum, FALSE);
    setInterrupts(ON);

    /* Return status */
    if (status != READY) {
        status = -status; 
    }
    return status;
}


/******************************************************************************
 * Function: diskPutSyscallHandler
 *
 * Description: Handles SYS14 (DISK_PUT). Acquires mutex, copies data from
 *              user to DMA buffer, calls diskRW, releases mutex.
 *
 * Parameters:
 *              supportStruct - Pointer to the current process's support structure
 *
 * Returns:
 *              Result of the diskRW operation (READY or negative error code)
 *              ERROR if parameters are invalid (terminates the process)
 *
 *****************************************************************************/
int diskPutSyscallHandler(support_PTR supportStruct) {
    /* Extract parameters */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
    memaddr logicalAddress = exceptState->s_a1;
    int diskNum = exceptState->s_a2;
    int linearSector = exceptState->s_a3;

    /* Validate parameters */
    if (diskNum <= 0 || diskNum >= DEV_PER_LINE || linearSector < 0 || !validateUserAddress(logicalAddress)) {
        terminateUProcess(NULL);
        return ERROR;
    }

    /* Get DMA buffer address */
    memaddr diskDmaBufferAddr = DISK_DMABUFFER_ADDR(diskNum);
    int devIndex = ((DISKINT - MAPINT) * DEV_PER_LINE) + (diskNum);

    /* Acquire device mutex */
    SYSCALL(PASSEREN, (int)&deviceMutex[devIndex], 0, 0);

    /* Copy data from user logical address to kernel DMA buffer */
    copyBlock((memaddr *)logicalAddress, (memaddr *)diskDmaBufferAddr);

    /* Perform the write operation using the helper (mutex is held) */
    int status = diskRW(WRITEBLK, diskNum, linearSector, diskDmaBufferAddr);

    /* Release device mutex */
    SYSCALL(VERHOGEN, (int)&deviceMutex[devIndex], 0, 0);

    return status;
}


/******************************************************************************
 * Function: diskGetSyscallHandler
 *
 * Description: Handles SYS15 (DISK_GET). Acquires mutex, calls diskRW,
 *              copies data from DMA buffer to user, releases mutex.
 *
 * Parameters:
 *              supportStruct - Pointer to the current process's support structure
 *
 * Returns:
 *              Result of the diskRW operation (READY or negative error code)
 *              ERROR if parameters are invalid (terminates the process)
 *
 *****************************************************************************/
int diskGetSyscallHandler(support_PTR supportStruct) {
    /* Extract parameters */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
    memaddr logicalAddress = exceptState->s_a1;
    int diskNum = exceptState->s_a2;
    int linearSector = exceptState->s_a3;

    /* Validate parameters */
    if (diskNum <= 0 || diskNum >= DEV_PER_LINE || linearSector < 0 || !validateUserAddress(logicalAddress)) {
        terminateUProcess(NULL);
        return ERROR;
    }

    /* Get DMA buffer address */
    memaddr diskDmaBufferAddr = DISK_DMABUFFER_ADDR(diskNum);
    int devIndex = ((DISKINT - MAPINT) * DEV_PER_LINE) + (diskNum);

    /* Acquire device mutex */
    SYSCALL(PASSEREN, (int)&deviceMutex[devIndex], 0, 0);

    /* Perform the read operation using the helper (mutex is held) */
    int status = diskRW(READBLK, diskNum, linearSector, diskDmaBufferAddr);

    /* If read was successful, copy data from DMA buffer to user (while mutex is held) */
    if (status == READY) {
        copyBlock((memaddr *)diskDmaBufferAddr, (memaddr *)logicalAddress);
    }

    /* Release device mutex */
    SYSCALL(VERHOGEN, (int)&deviceMutex[devIndex], 0, 0);

    return status;
}


/******************************************************************************
 * Function: flashPutSyscallHandler
 *
 * Description: Handles SYS16 (FLASH_PUT). Acquires mutex, copies data from
 *              user to DMA buffer, calls flashRW, releases mutex.
 *
 * Parameters:
 *              supportStruct - Pointer to the current process's support structure
 *
 * Returns:
 *              Result of the flashRW operation (READY or negative error code)
 *              ERROR if parameters are invalid (terminates the process)
 *
 *****************************************************************************/
int flashPutSyscallHandler(support_PTR supportStruct) {
    /* Extract parameters */
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
    int devIndex = ((FLASHINT - MAPINT) * DEV_PER_LINE) + (flashNum);

    /* Acquire device mutex */
    SYSCALL(PASSEREN, (int)&deviceMutex[devIndex], 0, 0);

    /* Copy data from user logical address to kernel DMA buffer */
    copyBlock((memaddr *)logicalAddress, (memaddr *)flashDmaBufferAddr);

    /* Call flashRW using the DMA buffer address (mutex is held) */
    int status = flashRW(WRITE, flashNum, blockNum, flashDmaBufferAddr);

    /* Release device mutex */
    SYSCALL(VERHOGEN, (int)&deviceMutex[devIndex], 0, 0);

    return status;
}

/******************************************************************************
 * Function: flashGetSyscallHandler
 *
 * Description: Handles SYS17 (FLASH_GET). Acquires mutex, calls flashRW,
 *              copies data from DMA buffer to user, releases mutex.
 *
 * Parameters:
 *              supportStruct - Pointer to the current process's support structure
 *
 * Returns:
 *              Result of the flashRW operation (READY or negative error code)
 *              ERROR if parameters are invalid (terminates the process)
 *
 *****************************************************************************/
int flashGetSyscallHandler(support_PTR supportStruct) {
    /* Extract parameters */
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
    int devIndex = ((FLASHINT - MAPINT) * DEV_PER_LINE) + (flashNum);

    /* Acquire device mutex */
    SYSCALL(PASSEREN, (int)&deviceMutex[devIndex], 0, 0);

    /* Call flashRW using the DMA buffer address (mutex is held) */
    int status = flashRW(READ, flashNum, blockNum, flashDmaBufferAddr);

    /* If read was successful, copy data from DMA buffer to user (while mutex is held) */
    if (status == READY) {
        copyBlock((memaddr *)flashDmaBufferAddr, (memaddr *)logicalAddress);
    }

    /* Release device mutex */
    SYSCALL(VERHOGEN, (int)&deviceMutex[devIndex], 0, 0);

    return status;
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
    int word; /* Copy data word by word */
    for (word = 0; word < (PAGESIZE / WORDLEN); word++) {
        *dest = *src;
        dest++;
        src++;
    }
}
