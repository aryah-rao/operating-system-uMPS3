/******************************* deviceSupport.c *********************************
 *
 * Module: DMA Device Support (Phase 4)
 *
 * Implements support for block-based DMA devices (disk and flash) and
 * provides handlers for SYS14–SYS17 system calls.
 *
 * Written by Aryah Rao & Anish Reddy
 *
 *****************************************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../h/sysSupport.h"
#include "../h/vmSupport.h"

#define DISK_CMD_READ   2
#define DISK_CMD_WRITE  3
#define FLASH_CMD_READ  2
#define FLASH_CMD_WRITE 3

/* External device mutex array and terminate function */
extern int deviceMutex[DEVICE_COUNT];
extern void terminateUProcess(int *mutex);
extern void setInterrupts(int toggle);

/* Helper: Validate user address is in KUSEG and 4KB aligned */
HIDDEN int valid_dma_addr(memaddr addr) {
    return (addr >= KUSEG) && ((addr & (PAGESIZE - 1)) == 0);
}

/* Helper: Get device register index for disk/flash */
HIDDEN int dev_index(int line, int devnum) {
    return ((line - MAPINT) * DEV_PER_LINE) + devnum;
}

/* Helper: Get pointer to device register */
HIDDEN device_t* get_devreg(int line, int devnum) {
    devregarea_t *devArea = (devregarea_t *)RAMBASEADDR;
    return &(devArea->devreg[dev_index(line, devnum)]);
}

/* ===================== SYS14: Disk Put (Write) ===================== */
void diskPutHandler(support_PTR supportStruct) {
    state_t *exceptState = &supportStruct->sup_exceptState[GENERALEXCEPT];
    memaddr buffer = exceptState->s_a1;
    int diskNum = exceptState->s_a2;
    int sectorNum = exceptState->s_a3;

    /* Validate parameters */
    if (!valid_dma_addr(buffer) || diskNum < 0 || diskNum >= DEV_PER_LINE || sectorNum < 0) {
        terminateUProcess(NULL);
    }

    int mutexIdx = dev_index(DISKINT, diskNum);
    device_t *dev = get_devreg(DISKINT, diskNum);

    /* Gain mutual exclusion */
    SYSCALL(PASSEREN, (int)&deviceMutex[mutexIdx], 0, 0);

    /* Set DMA address */
    dev->d_data0 = buffer;

    /* Prepare command: sectorNum in upper bits, command in lower */
    int command = (sectorNum << 8) | DISK_CMD_WRITE;

    /* Atomically write command and wait for I/O */
    setInterrupts(OFF);
    dev->d_command = command;
    int status = SYSCALL(WAITIO, DISKINT, diskNum, 0);
    setInterrupts(ON);

    /* Release mutual exclusion */
    SYSCALL(VERHOGEN, (int)&deviceMutex[mutexIdx], 0, 0);

    /* Return status: 1 if READY, else negative of status */
    exceptState->s_v0 = (status == READY) ? status : -status;
}

/* ===================== SYS15: Disk Get (Read) ===================== */
void diskGetHandler(support_PTR supportStruct) {
    state_t *exceptState = &supportStruct->sup_exceptState[GENERALEXCEPT];
    memaddr buffer = exceptState->s_a1;
    int diskNum = exceptState->s_a2;
    int sectorNum = exceptState->s_a3;

    /* Validate parameters */
    if (!valid_dma_addr(buffer) || diskNum < 0 || diskNum >= DEV_PER_LINE || sectorNum < 0) {
        terminateUProcess(NULL);
    }

    int mutexIdx = dev_index(DISKINT, diskNum);
    device_t *dev = get_devreg(DISKINT, diskNum);

    /* Gain mutual exclusion */
    SYSCALL(PASSEREN, (int)&deviceMutex[mutexIdx], 0, 0);

    /* Set DMA address */
    dev->d_data0 = buffer;

    /* Prepare command: sectorNum in upper bits, command in lower */
    int command = (sectorNum << 8) | DISK_CMD_READ;

    /* Atomically write command and wait for I/O */
    setInterrupts(OFF);
    dev->d_command = command;
    int status = SYSCALL(WAITIO, DISKINT, diskNum, 0);
    setInterrupts(ON);

    /* Release mutual exclusion */
    SYSCALL(VERHOGEN, (int)&deviceMutex[mutexIdx], 0, 0);

    /* Return status: 1 if READY, else negative of status */
    exceptState->s_v0 = (status == READY) ? status : -status;
}

/* ===================== SYS16: Flash Put (Write) ===================== */
void flashPutHandler(support_PTR supportStruct) {
    state_t *exceptState = &supportStruct->sup_exceptState[GENERALEXCEPT];
    memaddr buffer = exceptState->s_a1;
    int flashNum = exceptState->s_a2;
    int blockNum = exceptState->s_a3;

    /* Validate parameters */
    if (!valid_dma_addr(buffer) || flashNum < 0 || flashNum >= DEV_PER_LINE || blockNum < 32) {
        terminateUProcess(NULL);
    }

    /* Prevent writing to backing store blocks (0-31) */
    if (blockNum < 32) {
        terminateUProcess(NULL);
    }

    int mutexIdx = dev_index(FLASHINT, flashNum);
    device_t *dev = get_devreg(FLASHINT, flashNum);

    /* Gain mutual exclusion */
    SYSCALL(PASSEREN, (int)&deviceMutex[mutexIdx], 0, 0);

    /* Set DMA address */
    dev->d_data0 = buffer;

    /* Prepare command: blockNum in upper bits, command in lower */
    int command = (blockNum << 8) | FLASH_CMD_WRITE;

    /* Atomically write command and wait for I/O */
    setInterrupts(OFF);
    dev->d_command = command;
    int status = SYSCALL(WAITIO, FLASHINT, flashNum, 0);
    setInterrupts(ON);

    /* Release mutual exclusion */
    SYSCALL(VERHOGEN, (int)&deviceMutex[mutexIdx], 0, 0);

    /* Return status: 1 if READY, else negative of status */
    exceptState->s_v0 = (status == READY) ? status : -status;
}

/* ===================== SYS17: Flash Get (Read) ===================== */
void flashGetHandler(support_PTR supportStruct) {
    state_t *exceptState = &supportStruct->sup_exceptState[GENERALEXCEPT];
    memaddr buffer = exceptState->s_a1;
    int flashNum = exceptState->s_a2;
    int blockNum = exceptState->s_a3;

    /* Validate parameters */
    if (!valid_dma_addr(buffer) || flashNum < 0 || flashNum >= DEV_PER_LINE || blockNum < 32) {
        terminateUProcess(NULL);
    }

    /* Prevent reading from backing store blocks (0-31) */
    if (blockNum < 32) {
        terminateUProcess(NULL);
    }

    int mutexIdx = dev_index(FLASHINT, flashNum);
    device_t *dev = get_devreg(FLASHINT, flashNum);

    /* Gain mutual exclusion */
    SYSCALL(PASSEREN, (int)&deviceMutex[mutexIdx], 0, 0);

    /* Set DMA address */
    dev->d_data0 = buffer;

    /* Prepare command: blockNum in upper bits, command in lower */
    int command = (blockNum << 8) | FLASH_CMD_READ;

    /* Atomically write command and wait for I/O */
    setInterrupts(OFF);
    dev->d_command = command;
    int status = SYSCALL(WAITIO, FLASHINT, flashNum, 0);
    setInterrupts(ON);

    /* Release mutual exclusion */
    SYSCALL(VERHOGEN, (int)&deviceMutex[mutexIdx], 0, 0);

    /* Return status: 1 if READY, else negative of status */
    exceptState->s_v0 = (status == READY) ? status : -status;
}
