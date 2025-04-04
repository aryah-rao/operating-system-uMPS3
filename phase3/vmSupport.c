/*
 * vmSupport.c - Implementation of the Virtual Memory Support module (Level 4 / Phase 3).
 *
 * This module is responsible for providing virtual memory capabilities in the Pandos
 * operating system. It includes the Translation Lookaside Buffer (TLB) exception
 * handler (the Pager), initial functions for interacting with flash devices for
 * backing store, and management of the Swap Pool.
 *
 * Authors: Aryah Rao and Anish Reddy
 * Date: Current Date
 */

#include "../h/vmSupport.h"
#include "../h/sysSupport.h"

/* Macros for translating between addresses and page/frame numbers */
#define FRAME_TO_ADDR(frame) (RAMSTART + ((frame) * PAGESIZE))
#define ADDR_TO_FRAME(addr) (((addr) - RAMSTART) / PAGESIZE)

/* Next integer for FIFO replacement */
int nextFrameNum;

/* Forward declaration for functions in sysSupport.c */
extern void setInterrupts(int onOff);
extern void resumeState(state_PTR state);
extern void programTrapExceptionHandler();

/* Forward declaration for functions in scheduler.c */
extern void loadState(state_PTR state, int quantum);


/* The Pager */
void pager() {
    /* Get current process's support structure */
    support_PTR currentProcessSupport = (support_PTR)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    if (currentProcessSupport == NULL) {
        /* This shouldn't happen for a U-proc with virtual memory */
        programTrapExceptionHandler();
        return;
    }

    state_PTR exceptionState = &currentProcessSupport->sup_exceptState[PGFAULTEXCEPT];

    /* Why are we here? */
    int cause = (exceptionState->s_cause & CAUSE_EXCCODE_MASK) >> CAUSE_EXCCODE_SHIFT;

    /* If TLB-Modification */
    if ((cause != TLBINVLDL) && (cause != TLBINVLDS)) {
        /* NUKE IT! */
        programTrapExceptionHandler();
        return;
    }

    /* Gain swap pool mutual exclusion */
    SYSCALL(PASSEREN, &swapPoolMutex, 0, 0);
        
    /* Get page number (make this a macro?) */
    int pageNum = (((exceptionState->s_entryHI) & 0x3FFFF00) >> VPNSHIFT) % MAXPAGES;

    /* Validate the page number (should be between 0x80000000 and & 0x800000001E [31] or 0xBFFFFFFE) */


    /* Pick the next frame number */
    int frameNum = updateFrameNum();

    /* Get frame address */
    memaddr frameAddress = FRAME_TO_ADDR(frameNum);

    /* If frame number is occupied */
    if (swapPool[frameNum].valid) { /* Assume page is dirty */
        /* Interrupts off */
        setInterrupts(OFF);

        /* Update that U-proc's page table & TLB atomically */
        swapPool[frameNum].pte->pte_entryLO &= ~VALIDON; /* Clear the valid bit (CHANGE) */

        /* Clear the TLB (can optimize later) */
        TLBCLR();

        /* Enable interrupts */
        setInterrupts(ON);

        /* Write the page to backing store */
        int retCode = writeFlashPage(swapPool[frameNum].flash_dev, swapPool[frameNum].block_num, frameAddress);
        if (retCode != SUCCESS) {
            /* Error */
            programTrapExceptionHandler();
            return;
        }
    }

    /* Read the page from backing store */
    int flashDev, blockNum;
    int retCode = readFlashPage(pageNum, &flashDev, &blockNum);
    if (retCode != SUCCESS) {
        /* Error */
        programTrapExceptionHandler();
        return;
    }

    /* Update the swap pool entry */
    swapPool[frameNum].asid = currentProcessSupport->sup_asid;
    swapPool[frameNum].vpn = pageNum;
    swapPool[frameNum].pte = &currentProcessSupport->sup_pageTable[pageNum];
    swapPool[frameNum].flash_dev = flashDev;
    swapPool[frameNum].block_num = blockNum;
    swapPool[frameNum].valid = 1;

    /* Interrupts off */
    setInterrupts(OFF);

    /* Update this U-proc's page table and TLB atomically */
    swapPool[frameNum].pte->pte_entryLO = frameAddress | VALIDON | DIRTYON; /* Set the valid bit and dirty bit */

    /* Update TLB */


    /* Enable interrupts */
    setInterrupts(ON);

    /* Release swap pool mutual exclusion */
    SYSCALL(VERHOGEN, &swapPoolMutex, 0, 0);

    /* Retry the instruction */
    resumeState(exceptionState);
}


/* Done */
void uTLB_RefillHandler() {
    state_PTR exceptionState = (state_PTR )BIOSDATAPAGE;
    
    /* Get page number (make this a macro?) */
    int pageNum = (((exceptionState->s_entryHI) & 0x3FFFF00) >> VPNSHIFT) % MAXPAGES;

    /* Update the page table entry into the TLB */
    setENTRYHI(currentProcess->p_supportStruct->sup_pageTable[pageNum].pte_entryHI);
    setENTRYLO(currentProcess->p_supportStruct->sup_pageTable[pageNum].pte_entryLO);
    TLBWR();

    /* Restore the processor state */
    loadState(exceptionState, 0);
}

/* Done */
/* Public function to initialize the Swap Pool table and semaphore. */
void initSwapPool() {
    int i;
    for (i = 0; i < SWAPPOOLSIZE; i++) {
        swapPool[i].asid = 0;
        swapPool[i].vpn = 0;
        swapPool[i].flash_dev = 0;
        swapPool[i].block_num = 0;
        swapPool[i].valid = FALSE;
        swapPool[i].frame = 0;
        swapPool[i].pte = NULL;
    }
    
    /* Initialize the FIFO replacement pointer */
    nextFrameNum = 0;

    /* Initialize the Swap Pool semaphore */
    swapPoolMutex = 1;
}


/* Done */
/* HIDDEN function to allocate a physical frame - simplified FIFO approach */
HIDDEN int updateFrameNum() {
    /* Update the FIFO index for next time */
    nextFrameNum = (nextFrameNum + 1) % MAXPAGES;
    
    return nextFrameNum;
}

/* HIDDEN function to read a page from the flash device. */
HIDDEN int readFlashPage(int flashDev, int blockNum, memaddr physicalAddress) {
/*
     * Initiate a synchronous read operation on the specified flash device and block number.
     * This will involve using the SYS17 (FLASH GET) system call.
     * The data read from the flash device should be placed at the provided physicalAddress in RAM.
     * Return the completion status of the operation.
     * Handle potential errors (e.g., invalid flash device or block number) - though
     * such checks might be done by the caller (Pager).
*/
    return SYSCALL(DISK_GET, (unsigned int *)physicalAddress, flashDev, blockNum);
}

/* HIDDEN function to write a page to the flash device. */
HIDDEN int writeFlashPage(int flashDev, int blockNum, memaddr physicalAddress) {
    /*
     * Initiate a synchronous write operation on the specified flash device and block number.
     * This will involve using the SYS16flashDev, int blockNum, .
     * The data at the provided physicalAddress in RAM should be written to the flash device.
     * Return the completion status of the operation.
     * Handle potential errors (e.g., invalid flash device or block number) - though
     * such checks might be done by the caller (Pager).
    */
    return SYSCALL(DISK_PUT, (unsigned int *)physicalAddress, flashDev, blockNum);
}


/* Following the recommendation in pandos.pdf [Section 4.11.2] and the previous conversation,
 * the functions for SYS16 (FLASH PUT) and SYS17 (FLASH GET) should ideally be moved
 * to deviceSupportDMA.c in Phase 4 / Level 5 to promote code sharing.
 * However, for Phase 3 / Level 4, they might initially reside here, called by the Pager.
*/