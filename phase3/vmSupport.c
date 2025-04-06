/******************************* vmSupport.c *************************************
 *
 * Module: Virtual Memory Support
 *
 * Description:
 * This module is responsible for providing virtual memory capabilities in the
 * PandOS operating system. It implements the Translation Lookaside Buffer (TLB)
 * exception handler (the Pager), provides functions for interacting with flash
 * devices as backing store, and manages the Swap Pool for virtual memory pages.
 *
 * Implementation:
 * The module uses a FIFO approach for page replacement, maintains a swap pool
 * data structure to track virtual pages, and implements a TLB miss handler that
 * loads pages from flash devices on demand. It ensures mutual exclusion during
 * critical operations through semaphores and manages the mapping between virtual
 * and physical memory addresses.
 * 
 * Functions:
 * - pager: The main TLB exception handler that handles page faults.
 * - uTLB_RefillHandler: Fast TLB refill handler for TLB misses.
 * - initSwapPool: Initializes the swap pool data structures.
 * - updateFrameNum: Updates the frame number for FIFO page replacement.
 * - readFlashPage: Reads a page from flash device to RAM.
 * - writeFlashPage: Writes a page from RAM to flash device.
 *
 * Written by Aryah Rao and Anish Reddy
 *
 ***************************************************************************/

#include "../h/vmSupport.h"
#include "../h/sysSupport.h"

/* Next integer for FIFO replacement */
HIDDEN int nextFrameNum;
HIDDEN swapPoolEntry_t swapPool[SWAPPOOLSIZE];      /* Swap Pool data structure */
HIDDEN int swapPoolMutex;                           /* Semaphore for Swap Pool access */

/* Forward declaration for functions in sysSupport.c */
extern void programTrapExceptionHandler();
extern support_PTR getCurrentSupportStruct(); /* Get the current process's support structure */

/* Forward declaration for functions in scheduler.c */
extern void loadState(state_PTR state, int quantum);


/* ========================================================================
 * Function: pager
 *
 * Description: Main TLB exception handler that services page faults. Determines
 *              the cause of the fault, allocates physical frames using FIFO 
 *              replacement, and brings pages from backing store as needed.
 * 
 * Parameters:
 *              None (accesses exception state from support structure)
 * 
 * Returns:
 *              None (doesn't return directly - resumes execution with new state)
 * ======================================================================== */
void pager() {
    /* Get current process's support structure */
    support_PTR currentProcessSupport = (support_PTR)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    if (currentProcessSupport == NULL) {
        /* This shouldn't happen for a U-proc with virtual memory */
        programTrapExceptionHandler();
        return;
    }

    /* Get exception state */
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
/* ========================================================================
 * Function: uTLB_RefillHandler
 *
 * Description: Fast TLB refill handler that loads TLB entries directly
 *              from the page table when a TLB miss occurs but the page
 *              is already present in physical memory.
 * 
 * Parameters:
 *              None (accesses exception state from BIOS data page)
 * 
 * Returns:
 *              None (doesn't return directly - loads new processor state)
 * ======================================================================== */
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

/* Done*/
/* ========================================================================
 * Function: initSwapPool
 *
 * Description: Initializes the swap pool data structure and related semaphore.
 *              Sets all entries to an invalid state and prepares the FIFO
 *              replacement algorithm.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              None
 * ======================================================================== */
void initSwapPool() {
    int i;
    for (i = 0; i < SWAPPOOLSIZE; i++) {
        swapPool[i].asid = -1;
        swapPool[i].vpn = 0;
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
/* ========================================================================
 * Function: updateFrameNum
 *
 * Description: Updates the frame number for FIFO page replacement algorithm.
 *              Cycles through all available frames in sequence.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              The next frame number to be used
 * ======================================================================== */
HIDDEN int updateFrameNum() {
    /* Update the FIFO index for next time */
    nextFrameNum = (nextFrameNum + 1) % MAXPAGES;
    
    return nextFrameNum;
}


/* ========================================================================
 * Function: BackingStoreRW
 *
 * Description: Performs read or write operations on flash device backing store
 *              with proper mutual exclusion and atomicity. Calculates flash
 *              device and block number using available information.
 * 
 * Parameters:
 *              action - Indicates whether to read or write (DISK_GET or DISK_PUT)
 *              frameNum - The frame number in the swap pool
 *              vpn - Virtual page number (needed to calculate flash device/block)
 * 
 * Returns:
 *              SUCCESS if operation completed successfully
 *              ERROR if there was a problem
 * ======================================================================== */
HIDDEN int BackingStoreRW(int action, int frameNum, int vpn) {
    /* Calculate flash device and block number from vpn */
    int flashDev = vpn % DEVPERINT;  // Distribute across available devices
    int blockNum = vpn / DEVPERINT;  // Use blocks sequentially across devices
    
    devregarea_t* devRegisterArea;


    /* Calculate physical address from frame number */
    memaddr physicalAddress = FRAME_TO_ADDR(frameNum);
    
    /* Gain device mutex for the flash device */
    SYSCALL(PASSEREN, &deviceMutex[DEV_INDEX(FLASHINT, flashDev, FALSE)], 0, 0);
    
    /* Get device register address */
    dtpreg_t* flashReg = (dtpreg_t*)DEV_REG_ADDR(FLASHINT, flashDev);
    
    /* Set data register with physical address */
    flashReg->data0 = physicalAddress;
    
    /* Prepare command based on action */
    unsigned int command;
    if (action == DISK_GET) {
        command = (blockNum << 8) | READBLK;
    } else {
        command = (blockNum << 8) | WRITEBLK;
    }
    
    /* Execute command atomically */
    setInterrupts(OFF);
    flashReg->command = command;
    int status = SYSCALL(IOWAIT, FLASHINT, flashDev, FALSE);
    setInterrupts(ON);
    
    /* Release device mutex */
    SYSCALL(VERHOGEN, &deviceMutex[DEV_INDEX(FLASHINT, flashDev, FALSE)], 0, 0);
    
    /* Return status */
    return (status == READY) ? SUCCESS : ERROR;
}

/*----------------------------------------------------------------------------*/
/* Helper Functions (Implementation) */
/*----------------------------------------------------------------------------*/

/* Helper function to validate user addresses */
HIDDEN int validateUserAddress(void *address) {
    /* Check if address is in user space (KUSEG) */
    return (address >= KUSEG && address < (KUSEG + (PAGESIZE * MAXPAGES)));
}

/* Terminate the current process with proper cleanup */
HIDDEN void terminateUProc() {
    /* Clear the current proceess's pages in swap pool */
    clearSwapPoolEntries();

    /* Release swap pool mutex if held (idk how to do this) */

    
    /* Update master semaphore to indicate process termination */
    SYSCALL(VERHOGEN, &masterSema4, 0, 0);

    /* Terminate the process */
    SYSCALL(TERMINATEPROCESS, 0, 0, 0);
}


/* Done */
/* ========================================================================
 * Function: setInterrupts
 *
 * Description: Enables or disables interrupts by modifying the status register
 *              based on the onOff parameter.
 * 
 * Parameters:
 *              toggle - ON to enable interrupts, OFF to disable interrupts
 * 
 * Returns:
 *              None
 * ======================================================================== */
void setInterrupts(int toggle) {
    /* Get current status register value */
    unsigned int status = getSTATUS();

    if (toggle == ON) {
        /* Enable interrupts */
        status |= STATUS_IEc;
    } else {
        /* Disable interrupts */
        status &= ~STATUS_IEc;
    }
    /* Set the status register */
    setSTATUS(status);
}

/* Done */
/* ========================================================================
 * Function: resumeState
 *
 * Description: Loads a processor state using the LDST instruction.
 *              This is a wrapper around the LDST macro for cleaner code.
 * 
 * Parameters:
 *              state - Pointer to the processor state to load
 * 
 * Returns:
 *              Does not return (control passes to the loaded state)
 * ======================================================================== */
void resumeState(state_t *state) {
    /* Check if state pointer is NULL */
    if (state == NULL) {
        /* Critical error - state is NULL */
        PANIC();
    }

    /* Load the state into processor */
    LDST(state);
    
    /* Control never returns here */
}

/* Done */
/* ========================================================================
 * Function: clearSwapPoolEntries
 *
 * Description: Clears all swap pool entries for the current process.
 *              This function is called when a process terminates.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              None
 * ======================================================================== */
void clearSwapPoolEntries(int asid) {
    /* Get the current process's ASID */
    int asid = getCurrentSupportStruct()->sup_asid;
    if (asid <= 0) {
        /* Invalid ASID, nothing to clear */
        return;
    }

    if (swapPool == NULL) {
        /* Swap pool not initialized */
        return;
    }

    /* Acquire the Swap Pool semaphore (SYS3) */
    SYSCALL(PASSEREN, &swapPoolMutex, 0, 0);

    /* Disable interrupts during critical section */
    setInterrupts(OFF);

    /* Clear all swap pool entries for the current process */
    int i;
    for (i = 0; i < SWAPPOOLSIZE; i++) {
        if (swapPool[i].asid == asid) {
            swapPool[i].valid = FALSE;
        }
    }

    /* Re-enable interrupts */
    setInterrupts(ON);

    /* Release the Swap Pool semaphore (SYS4) */
    SYSCALL(VERHOGEN, &swapPoolMutex, 0, 0);


}

/* Following the recommendation in pandos.pdf [Section 4.11.2] and the previous conversation,
 * the functions for SYS16 (FLASH PUT) and SYS17 (FLASH GET) should ideally be moved
 * to deviceSupportDMA.c in Phase 4 / Level 5 to promote code sharing.
 * However, for Phase 3 / Level 4, they might initially reside here, called by the Pager.
*/