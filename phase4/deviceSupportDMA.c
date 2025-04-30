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
 * - kernel_memcpy: Helper for word-aligned memory copy.
 * - valid_dma_addr: Helper to validate user address for DMA syscalls.
 * - flashRW: Performs raw read/write to flash device (used by syscall handlers
 *            and potentially the pager).
 * - diskPutSyscallHandler: Implements SYS14 (DISK_PUT).
 * - diskGetSyscallHandler: Implements SYS15 (DISK_GET).
 * - flashPutSyscallHandler: Implements SYS16 (FLASH_PUT).
 * - flashGetSyscallHandler: Implements SYS17 (FLASH_GET).
 *
 * Written by Aryah Rao & Anish Reddy
 *
 *****************************************************************************/

#include "../h/deviceSupportDMA.h" /* Include this module's header */

/*----------------------------------------------------------------------------*/
/* Helper Function Declarations */
/*----------------------------------------------------------------------------*/

HIDDEN void kernel_memcpy(void *dest, const void *src, unsigned int n_bytes);

/*----------------------------------------------------------------------------*/
/* Global Function Implementations */
/*----------------------------------------------------------------------------*/

/******************************************************************************
 * Function: flashRW
 *
 * Description: Performs a raw read or write operation on a flash device using
 *              the provided physical buffer address directly for the device's
 *              DMA register. Handles device mutex acquisition/release and
 *              atomicity for the command issue and WAITIO.
 *              This function is intended for use by syscall handlers (which
 *              provide a DMA buffer address) and the pager (which provides
 *              a physical frame address).
 *
 * Parameters:
 *              operation - READ (2) or WRITE (3).
 *              flashNum - Flash device number (0-7).
 *              blockNum - Block number on the flash device.
 *              bufferAddr - The physical address to use for the device's d_data0.
 *
 * Returns:
 *              READY (1) on successful completion.
 *              Negative value of the device status code on error.
 *              ERROR (-1) for invalid input parameters (flashNum, blockNum).
 *****************************************************************************/
int flashRW(int operation, int flashNum, int blockNum, memaddr bufferAddr) {
    /* Access device registers */
    devregarea_t *devRegisterArea = (devregarea_t *)RAMBASEADDR;
    int devIndex = ((FLASHINT - MAPINT) * DEV_PER_LINE) + (flashNum);

    /* Gain device mutex */
    SYSCALL(PASSEREN, (int)&deviceMutex[devIndex], 0, 0);

    /* Perform atomic device operation */
    setInterrupts(OFF);
    devRegisterArea->devreg[devIndex].d_data0 = bufferAddr;
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
 *              block (linear sector number) using a dedicated kernel DMA buffer.
 *              Performs SEEKCYL then WRITEBLK.
 *
 * Parameters:
 *              supportStruct - Pointer to the current process's support structure.
 *
 * Returns:
 *              READY (1) on successful completion.
 *              Negative value of the device status code on error.
 *              (This function terminates the process on invalid parameters).
 *****************************************************************************/
int diskPutSyscallHandler(support_PTR supportStruct) {
    /* Extract parameters from the exception state */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
    memaddr logicalAddress = exceptState->s_a1;
    int diskNum = exceptState->s_a2;
    int linearSector = exceptState->s_a3; /* Treat s_a3 as a linear sector number */
    /* Access device registers directly */
    devregarea_t *devRegisterArea = (devregarea_t *)RAMBASEADDR;
    int devIndex = ((DISKINT - MAPINT) * DEV_PER_LINE) + (diskNum);
    device_t *devReg = &(devRegisterArea->devreg[devIndex]); /* Pointer to device register */
    int status; /* Declare status */
    unsigned int geometry, max_cyl, max_head, max_sect, sectors_per_cyl;
    unsigned int cyl, head, sect;

    /* Validate basic parameters */
    if (diskNum < 0 || diskNum >= DEV_PER_LINE || linearSector < 0 || logicalAddress < KUSEG) {
        terminateUProcess(NULL);
        return -1; /* Should not be reached */
    }

    /* Get DMA buffer address */
    memaddr diskDmaBufferAddr = DISK_DMABUFFER_ADDR(diskNum);

    /* Gain device mutex */
    SYSCALL(PASSEREN, (int)&deviceMutex[devIndex], 0, 0);

    /* --- Get Disk Geometry and Calculate C/H/S --- */
    geometry = devReg->d_data1; /* Read geometry from DATA1 */
    max_cyl = (geometry >> 24) & 0xFF;
    max_head = (geometry >> 16) & 0xFF;
    max_sect = (geometry >> 8) & 0xFF;


    sectors_per_cyl = max_head * max_sect;


    cyl = linearSector / sectors_per_cyl;
    unsigned int temp = linearSector % sectors_per_cyl;
    head = temp / max_sect;
    sect = temp % max_sect;

    /* --- End Geometry Calculation --- */


    /* --- Perform SEEKCYL Operation --- */
    setInterrupts(OFF);
    devReg->d_command = (cyl << 8) | SEEKCYL; /* SEEKCYL command */
    status = SYSCALL(WAITIO, DISKINT, diskNum, 0);
    setInterrupts(ON);

    if (status != READY) {
        SYSCALL(VERHOGEN, (int)&deviceMutex[devIndex], 0, 0); /* Release mutex */
        return -status; /* Return SEEK error */
    }
    /* --- End SEEKCYL Operation --- */


    /* --- Perform WRITEBLK Operation --- */
    /* Copy data from user buffer to kernel DMA buffer */
    kernel_memcpy((void *)diskDmaBufferAddr, (void *)logicalAddress, PAGESIZE);

    setInterrupts(OFF);
    devReg->d_data0 = diskDmaBufferAddr; /* Set DMA buffer address */
    /* WRITEBLK command: head in bits 16-23, sect in 8-15, command code 4 */
    devReg->d_command = (head << 16) | (sect << 8) | WRITEBLK;
    status = SYSCALL(WAITIO, DISKINT, diskNum, 0);
    setInterrupts(ON);
    /* --- End WRITEBLK Operation --- */


    /* Release device mutex */
    SYSCALL(VERHOGEN, (int)&deviceMutex[devIndex], 0, 0);

    /* Return final status (from WRITEBLK) */
    if (status != READY) {
        status = -status;
    }
    return status;
}


/******************************************************************************
 * Function: diskGetSyscallHandler
 *
 * Description: Handles the SYS15 (DISK_GET) system call. Reads a page of data
 *              from the specified disk block (linear sector number) into a
 *              dedicated kernel DMA buffer, then copies it to the user-provided
 *              logical address. Performs SEEKCYL then READBLK.
 *
 * Parameters:
 *              supportStruct - Pointer to the current process's support structure.
 *
 * Returns:
 *              READY (1) on successful completion.
 *              Negative value of the device status code on error.
 *              (This function terminates the process on invalid parameters).
 *****************************************************************************/
int diskGetSyscallHandler(support_PTR supportStruct) {
    /* Extract parameters from the exception state */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
    memaddr logicalAddress = exceptState->s_a1;
    int diskNum = exceptState->s_a2;
    int linearSector = exceptState->s_a3; /* Treat s_a3 as a linear sector number */
    /* Access device registers directly */
    devregarea_t *devRegisterArea = (devregarea_t *)RAMBASEADDR;
    int devIndex = ((DISKINT - MAPINT) * DEV_PER_LINE) + (diskNum);
    device_t *devReg = &(devRegisterArea->devreg[devIndex]); /* Pointer to device register */
    int status; /* Declare status */
    unsigned int geometry, max_cyl, max_head, max_sect, sectors_per_cyl;
    unsigned int cyl, head, sect;

    /* Validate basic parameters */
    if (diskNum < 0 || diskNum >= DEV_PER_LINE || linearSector < 0 || logicalAddress < KUSEG) {
        terminateUProcess(NULL);
        return -1; /* Should not be reached */
    }

    /* Get DMA buffer address */
    memaddr diskDmaBufferAddr = DISK_DMABUFFER_ADDR(diskNum);

    /* Gain device mutex */
    SYSCALL(PASSEREN, (int)&deviceMutex[devIndex], 0, 0);

    /* --- Get Disk Geometry and Calculate C/H/S --- */
    geometry = devReg->d_data1; /* Read geometry from DATA1 */
    max_cyl = (geometry >> 24) & 0xFF;
    max_head = (geometry >> 16) & 0xFF;
    max_sect = (geometry >> 8) & 0xFF;

    sectors_per_cyl = max_head * max_sect;


    cyl = linearSector / sectors_per_cyl;
    unsigned int temp = linearSector % sectors_per_cyl;
    head = temp / max_sect;
    sect = temp % max_sect;

    /* --- End Geometry Calculation --- */


    /* --- Perform SEEKCYL Operation --- */
    setInterrupts(OFF);
    devReg->d_command = (cyl << 8) | SEEKCYL; /* SEEKCYL command */
    status = SYSCALL(WAITIO, DISKINT, diskNum, 0);
    setInterrupts(ON);

    if (status != READY) {
        SYSCALL(VERHOGEN, (int)&deviceMutex[devIndex], 0, 0); /* Release mutex */
        return -status; /* Return SEEK error */
    }
    /* --- End SEEKCYL Operation --- */


    /* --- Perform READBLK Operation --- */
    setInterrupts(OFF);
    devReg->d_data0 = diskDmaBufferAddr; /* Set DMA buffer address */
    /* READBLK command: head in bits 16-23, sect in 8-15, command code 3 */
    devReg->d_command = (head << 16) | (sect << 8) | READBLK;
    status = SYSCALL(WAITIO, DISKINT, diskNum, 0);
    setInterrupts(ON);
    /* --- End READBLK Operation --- */


    /* Release device mutex - BEFORE potential copy */
    SYSCALL(VERHOGEN, (int)&deviceMutex[devIndex], 0, 0);

    /* Check status from READBLK */
    if (status != READY) {
        return -status; /* Return READBLK error */
    }

    /* If READBLK was successful, copy data from kernel DMA buffer to user buffer */
    kernel_memcpy((void *)logicalAddress, (void *)diskDmaBufferAddr, PAGESIZE);

    return status; /* Return READY status */
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
 * 
 *****************************************************************************/
int flashPutSyscallHandler(support_PTR supportStruct) {
    /* Extract parameters from the exception state */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
    memaddr logicalAddress = exceptState->s_a1;
    int flashNum = exceptState->s_a2;
    int blockNum = exceptState->s_a3;

    /* Validate parameters (including backing store check) */
    if (flashNum < 0 || flashNum >= DEV_PER_LINE || blockNum < 32 || logicalAddress < KUSEG) {
        terminateUProcess(NULL);
    }

    /* Copy data from user buffer to kernel DMA buffer */
    memaddr flashDmaBufferAddr = FLASH_DMABUFFER_ADDR(flashNum);
    kernel_memcpy((void *)flashDmaBufferAddr, (void *)logicalAddress, PAGESIZE);

    /* Call flashRW using the DMA buffer address */
    int status = flashRW(WRITE, flashNum, blockNum, flashDmaBufferAddr);
    return status;
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
 * 
 *****************************************************************************/
int flashGetSyscallHandler(support_PTR supportStruct) {
    /* Extract parameters from the exception state */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
    memaddr logicalAddress = exceptState->s_a1;
    int flashNum = exceptState->s_a2;
    int blockNum = exceptState->s_a3;

    /* Validate parameters (including backing store check) */
    if (flashNum < 0 || flashNum >= DEV_PER_LINE || blockNum < 32 || logicalAddress < KUSEG) {
        terminateUProcess(NULL);
    }

    /* Call flashRW using the DMA buffer address */
    memaddr flashDmaBufferAddr = FLASH_DMABUFFER_ADDR(flashNum);
    int status = flashRW(READ, flashNum, blockNum, flashDmaBufferAddr);

    /* If read was successful, copy data from DMA buffer to user buffer */
    if (status == READY) {
        kernel_memcpy((void *)logicalAddress, (void *)flashDmaBufferAddr, PAGESIZE);
    }
    return status;
}

/*----------------------------------------------------------------------------*/
/* Helper Function Implementations */
/*----------------------------------------------------------------------------*/

/******************************************************************************
 * Function: kernel_memcpy
 *
 * Description: Copies a specified number of bytes from source to destination.
 *              Optimized for word-aligned transfers but handles any remaining
 *              bytes individually. Assumes the system can handle potential
 *              page faults if src or dest point to user space.
 *
 * Parameters:
 *              dest - Pointer to the destination buffer.
 *              src - Pointer to the source buffer.
 *              n_bytes - Number of bytes to copy.
 *
 * Returns:
 *              None.
 *****************************************************************************/
HIDDEN void kernel_memcpy(void *dest, const void *src, unsigned int n_bytes) {
    unsigned int *d_word = (unsigned int *)dest;
    const unsigned int *s_word = (const unsigned int *)src;
    unsigned int n_words = n_bytes / WORDLEN;

    unsigned int i;

    /* Copy word-aligned portion */
    for (i = 0; i < n_words; i++) {
        d_word[i] = s_word[i];
    }

    /* Handle remaining bytes if n_bytes is not a multiple of WORDLEN */
    unsigned int bytes_remaining = n_bytes % WORDLEN;
    if (bytes_remaining > 0) {
        /* Calculate pointers to the start of the remaining bytes */
        unsigned char *d_byte = (unsigned char *)dest + (n_words * WORDLEN);
        const unsigned char *s_byte = (const unsigned char *)src + (n_words * WORDLEN);

        /* Copy remaining bytes one by one */
        for (i = 0; i < bytes_remaining; i++) {
            d_byte[i] = s_byte[i];
        }
    }
}