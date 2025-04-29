
#include "../h/const.h"
#include "../h/types.h"
#include "../h/deviceSupport.h"
#include "../h/vmSupport.h"

#ifndef DEV_REG_ADDR
#define DEV_REG_ADDR(intLineNo, devNo) ((memaddr)(RAMBASEADDR + (0x80 * ((intLineNo - 3) * DEV_PER_LINE + devNo))))
#endif

#define FLASH_DEV_REG(flashNum) ((device_t *)DEV_REG_ADDR(FLASHINT, (flashNum)))
#define DISK_DEV_REG(diskNum) ((device_t *)DEV_REG_ADDR(DISKINT, (diskNum)))



int flashRW(int operation, int flashNum, int blockNum, memaddr ramAddr) {
    device_t *dev = FLASH_DEV_REG(flashNum);

    int devMutex = ((FLASHINT - MAPINT) * DEV_PER_LINE) + (flashNum);

    SYSCALL(PASSEREN, (int)&deviceMutex[devMutex], 0, 0);

    setInterrupts(OFF);

    dev->d_data0 = ramAddr;
    dev->d_command = (blockNum << 8) | operation;

    int status = SYSCALL(WAITIO, FLASHINT, flashNum, FALSE);

    setInterrupts(ON);

    SYSCALL(VERHOGEN, (int)&deviceMutex[devMutex], 0, 0);

    return status;
}



int diskPutSyscallHandler(void) {
    support_t *supportStruct = getCurrentSupportStruct();
    state_t *state = &supportStruct->sup_exceptState[GENERALEXCEPT];
    memaddr ramAddr = state->s_a1;
    int diskNum = state->s_a2;
    int sect = state->s_a3;

    if (diskNum < 0 || sect < 0 || ramAddr == 0)
        return -1;

    device_t *dev = DISK_DEV_REG(diskNum);

    int devMutex = ((DISKINT - MAPINT) * DEV_PER_LINE) + (diskNum);

    SYSCALL(PASSEREN, (int)&deviceMutex[devMutex], 0, 0);

    setInterrupts(OFF);

    dev->d_data0 = ramAddr;
    dev->d_command = (sect << 8) | WRITE;

    int status = SYSCALL(WAITIO, DISKINT, diskNum, 0);

    setInterrupts(ON);

    SYSCALL(VERHOGEN, (int)&deviceMutex[devMutex], 0, 0);

    return (status == READY) ? 0 : -1;
}

int diskGetSyscallHandler(void) {
    support_t *supportStruct = getCurrentSupportStruct();
    state_t *state = &supportStruct->sup_exceptState[GENERALEXCEPT];
    memaddr ramAddr = state->s_a1;
    int diskNum = state->s_a2;
    int sect = state->s_a3;

    if (diskNum < 0 || sect < 0 || ramAddr == 0)
        return -1;

    device_t *dev = DISK_DEV_REG(diskNum);

    int devMutex = ((DISKINT - MAPINT) * DEV_PER_LINE) + (diskNum);

    SYSCALL(PASSEREN, (int)&deviceMutex[devMutex], 0, 0);

    setInterrupts(OFF);

    dev->d_data0 = ramAddr;
    dev->d_command = (sect << 8) | READ;

    int status = SYSCALL(WAITIO, DISKINT, diskNum, 0);

    setInterrupts(ON);

    SYSCALL(VERHOGEN, (int)&deviceMutex[devMutex], 0, 0);

    return (status == READY) ? 0 : -1;
}



int flashPutSyscallHandler(void) {
    support_t *supportStruct = getCurrentSupportStruct();
    state_t *state = &supportStruct->sup_exceptState[GENERALEXCEPT];
    memaddr ramAddr = state->s_a1;
    int flashNum = state->s_a2;
    int blockNum = state->s_a3;

    if (flashNum < 0 || blockNum < 0 || ramAddr == 0)
        return -1;

    int status = flashRW(WRITE, flashNum, blockNum, ramAddr);
    return (status == READY) ? 0 : -1;
}

int flashGetSyscallHandler(void) {
    support_t *supportStruct = getCurrentSupportStruct();
    state_t *state = &supportStruct->sup_exceptState[GENERALEXCEPT];
    memaddr ramAddr = state->s_a1;
    int flashNum = state->s_a2;
    int blockNum = state->s_a3;

    if (flashNum < 0 || blockNum < 0 || ramAddr == 0)
        return -1;

    int status = flashRW(READ, flashNum, blockNum, ramAddr);
    return (status == READY) ? 0 : -1;
}
