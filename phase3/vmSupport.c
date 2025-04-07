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
 * - backingStoreRW: Performs atomic read/write operations on flash devices.
 * - validateUserAddress: Validates if an address is in user space.
 * - terminateUProc: Terminates a user process with proper cleanup.
 * - setInterrupts: Enables or disables interrupts.
 * - resumeState: Resumes execution with a given processor state.
 * - clearSwapPoolEntries: Clears swap pool entries for a given ASID.
 *
 * Written by Aryah Rao and Anish Reddy
 *
 ***************************************************************************/

#include "../h/vmSupport.h"

/*----------------------------------------------------------------------------*/
/* Module level variables */
/*----------------------------------------------------------------------------*/

/* Next integer for FIFO replacement */
HIDDEN int nextFrameNum;
HIDDEN swapPoolEntry_t swapPool[SWAPPOOLSIZE]; /* Swap Pool data structure */
HIDDEN int swapPoolMutex;                      /* Semaphore for Swap Pool access */

/*----------------------------------------------------------------------------*/
/* Helper Function Prototypes (HIDDEN functions) */
/*----------------------------------------------------------------------------*/
HIDDEN int updateFrameNum();
HIDDEN int backingStoreRW(int operation, int frameNum);

/* Forward declaration for functions in sysSupport.c */
extern void programTrapExceptionHandler();
extern support_PTR getCurrentSupportStruct(); /* Get the current process's support structure */

/* Forward declaration for functions in scheduler.c */
extern void loadProcessState(state_PTR state, unsigned int quantum);

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
void pager()
{
    /* Get current process's support structure */
    support_PTR currentProcessSupport = (support_PTR)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    if (currentProcessSupport == NULL)
    {
        /* This shouldn't happen for a U-proc with virtual memory */
        programTrapExceptionHandler();
        return;
    }

    /* Get exception state */
    state_PTR exceptionState = &currentProcessSupport->sup_exceptState[PGFAULTEXCEPT];

    /* Why are we here? */
    int cause = (exceptionState->s_cause & CAUSE_EXCCODE_MASK) >> CAUSE_EXCCODE_SHIFT;

    /* If TLB-Modification */
    if ((cause != TLBINVLDL) && (cause != TLBINVLDS))
    {
        /* NUKE IT! */
        programTrapExceptionHandler();
        return;
    }

    /* Gain swap pool mutual exclusion */
    SYSCALL(PASSEREN, (unsigned int)&swapPoolMutex, 0, 0);

    /* Get page number (make this a macro?) */
    int pageNum = (((exceptionState->s_entryHI) & 0x3FFFF00) >> VPNSHIFT) % MAXPAGES;

    /* Validate the page number (should be between 0x80000000 and & 0x800000001E [31] or 0xBFFFFFFE) */


    /* Pick the next frame number */
    int frameNum = updateFrameNum();

    /* Get frame address */
    memaddr frameAddress = FRAME_TO_ADDR(frameNum);

    /* If frame number is occupied */
    if (swapPool[frameNum].valid)
    { /* Assume page is dirty */
        /* Interrupts off */
        setInterrupts(OFF);

        /* Update that U-proc's page table & TLB atomically */
        swapPool[frameNum].pte->pte_entryLO &= ~VALIDON; /* Clear the valid bit (CHANGE) */

        /* Clear the TLB (can optimize later) */
        TLBCLR();

        /* Enable interrupts */
        setInterrupts(ON);

        /* Write the page to backing store */
        int status = backingStoreRW(WRITE, frameNum);
        if (status != SUCCESS)
        {
            terminateUProc(&swapPoolMutex);
            return;
        }
    }

    /* Read the page from backing store */
    int retCode = backingStoreRW(READ, frameNum);
    if (retCode != SUCCESS)
    {
        terminateUProc(&swapPoolMutex);
        return;
    }

    /* Update the swap pool entry */
    swapPool[frameNum].asid = currentProcessSupport->sup_asid;
    swapPool[frameNum].vpn = pageNum;
    swapPool[frameNum].pte = &currentProcessSupport->sup_pageTable[pageNum];
    swapPool[frameNum].valid = 1;

    /* Interrupts off */
    setInterrupts(OFF);

    /* Update this U-proc's page table and TLB atomically */
    swapPool[frameNum].pte->pte_entryLO = frameAddress | VALIDON | DIRTYON; /* Set the valid bit and dirty bit */

    /* Update TLB */
    TLBCLR(); /* Clear the TLB to ensure it will use the new entry */

    /* Enable interrupts */
    setInterrupts(ON);

    /* Release swap pool mutual exclusion */
    SYSCALL(VERHOGEN, (unsigned int)&swapPoolMutex, 0, 0);

    /* Retry the instruction */
    resumeState(exceptionState);
}

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
void uTLB_RefillHandler()
{
    state_PTR exceptionState = (state_PTR)BIOSDATAPAGE;

    /* Get page number (make this a macro?) */
    int pageNum = (((exceptionState->s_entryHI) & 0x3FFFF00) >> VPNSHIFT) % MAXPAGES;

    /* Update the page table entry into the TLB */
    setENTRYHI(currentProcess->p_supportStruct->sup_pageTable[pageNum].pte_entryHI);
    setENTRYLO(currentProcess->p_supportStruct->sup_pageTable[pageNum].pte_entryLO);
    TLBWR();

    /* Restore the processor state */
    loadProcessState(exceptionState, 0);
}

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
void initSwapPool()
{
    int i;
    for (i = 0; i < SWAPPOOLSIZE; i++)
    {
        swapPool[i].asid = -1;
        swapPool[i].vpn = FALSE;
        swapPool[i].valid = FALSE;
        swapPool[i].pte = NULL;
    }

    /* Initialize the FIFO replacement pointer */
    nextFrameNum = 0;

    /* Initialize the Swap Pool semaphore */
    swapPoolMutex = 1;
}

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
HIDDEN int updateFrameNum()
{
    /* Update the FIFO index for next time */
    nextFrameNum = (nextFrameNum + 1) % MAXPAGES;

    return nextFrameNum;
}

/* ========================================================================
 * Function: backingStoreRW
 *
 * Description: Performs read or write operations on flash device backing store
 *              with proper mutual exclusion and atomicity. Calculates flash
 *              device and block number using available information.
 *
 * Parameters:
 *              operation - The operation to perform (READ or WRITE)
 *              frameNum - The frame number to operate on
 *
 * Returns:
 *              SUCCESS if operation completed successfully
 *              ERROR if there was a problem
 * ======================================================================== */
HIDDEN int backingStoreRW(int operation, int frameNum) {
    int frameAddress = FRAME_TO_ADDR(frameNum); /* Convert frame number to frame address in the swap pool */
    int flashNum = swapPool[frameNum].asid-1; /* Get the flash device number from the ASID */

    devregarea_t *devRegisterArea = (devregarea_t *)RAMBASEADDR; /* Access device registers from RAMBASE address */

    /* Calculate device semaphore index based on line and device number */
    int devSemaphore = ((FLASHINT - MAPINT) * DEV_PER_LINE) + (flashNum);

    /* Gain device mutex for the flash device */
    SYSCALL(PASSEREN, (unsigned int)&deviceMutex[devSemaphore], 0, 0);

    /* Atomically read a page from the flash device */
    setInterrupts(OFF); /* Disable interrupts to ensure atomicity */
    devRegisterArea->devreg[devSemaphore].d_data0 = frameAddress; /* Memory address */
    devRegisterArea->devreg[devSemaphore].d_command = operation; /* Operation type */
    int status = SYSCALL(WAITIO, FLASHINT, flashNum, FALSE); /* Wait for operation to complete */
    setInterrupts(ON); /* Re-enable interrupts */

    /* Release device mutex for the flash device */
    SYSCALL(VERHOGEN, (unsigned int)&deviceMutex[devSemaphore], 0, 0);

    /* Return the status of the operation */
    return status;
}

/*----------------------------------------------------------------------------*/
/* Helper Functions (Implementation) */
/*----------------------------------------------------------------------------*/

/* ========================================================================
 * Function: validateUserAddress
 *
 * Description: Validates if a given memory address is within user space.
 *              Only addresses within the KUSEG range are considered valid.
 *
 * Parameters:
 *              address - Pointer to memory address to validate
 *
 * Returns:
 *              TRUE if address is valid user space address
 *              FALSE if address is invalid
 * ======================================================================== */
extern int validateUserAddress(unsigned int address)
{
    /* Check if address is in user space (KUSEG) */
    return (address >= KUSEG && address < (KUSEG + (PAGESIZE * MAXPAGES)));
}

/* ========================================================================
 * Function: terminateUProc
 *
 * Description: Terminates the current user process with proper cleanup.
 *              Releases any held mutexes and updates the master semaphore.
 *
 * Parameters:
 *              mutex - Semaphore that needs to be released (or NULL)
 *
 * Returns:
 *              None (doesn't return - terminates process)
 * ======================================================================== */
extern void terminateUProc(int *mutex)
{
    /* Clear the current proceess's pages in swap pool */
    clearSwapPoolEntries(getCurrentSupportStruct()->sup_asid);

    /* Release mutex if held */
    if (mutex != NULL) {
        SYSCALL(VERHOGEN, (unsigned int)&mutex, 0, 0);
    }

    /* Update master semaphore to indicate process termination */
    SYSCALL(VERHOGEN, (unsigned int)&masterSema4, 0, 0);

    /* Terminate the process */
    SYSCALL(TERMINATEPROCESS, 0, 0, 0);
}

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
void setInterrupts(int toggle)
{
    /* Get current status register value */
    unsigned int status = getSTATUS();

    if (toggle == ON)
    {
        /* Enable interrupts */
        status |= STATUS_IEc;
    }
    else
    {
        /* Disable interrupts */
        status &= ~STATUS_IEc;
    }
    /* Set the status register */
    setSTATUS(status);
}

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
void resumeState(state_t *state)
{
    /* Check if state pointer is NULL */
    if (state == NULL)
    {
        /* Critical error - state is NULL */
        PANIC();
    }

    /* Load the state into processor */
    LDST(state);

    /* Control never returns here */
}

/* ========================================================================
 * Function: clearSwapPoolEntries
 *
 * Description: Clears all swap pool entries for a specific process identified
 *              by its ASID. Used during process termination to free resources.
 *
 * Parameters:
 *              asid - Address Space ID of the process
 *
 * Returns:
 *              None
 * ======================================================================== */
void clearSwapPoolEntries(int asid)
{
    /* Get the current process's ASID */
    int processAsid = asid;
    if (processAsid <= 0)
    {
        /* Invalid ASID, nothing to clear */
        return;
    }

    if (swapPool == NULL)
    {
        /* Swap pool not initialized */
        return;
    }

    /* Acquire the Swap Pool semaphore (SYS3) */
    SYSCALL(PASSEREN, (unsigned int)&swapPoolMutex, 0, 0);

    /* Disable interrupts during critical section */
    setInterrupts(OFF);

    /* Clear all swap pool entries for the current process */
    int i;
    for (i = 0; i < SWAPPOOLSIZE; i++)
    {
        if (swapPool[i].asid == processAsid)
        {
            swapPool[i].valid = FALSE;
        }
    }

    /* Re-enable interrupts */
    setInterrupts(ON);

    /* Release the Swap Pool semaphore (SYS4) */
    SYSCALL(VERHOGEN, (unsigned int)&swapPoolMutex, 0, 0);
}

