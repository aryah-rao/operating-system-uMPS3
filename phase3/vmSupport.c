/******************************* vmSupport.c *************************************
 *
 * Module: Virtual Memory Support
 *
 * Description:
 * This module implements the Support Level's virtual memory management system,
 * including the pager for handling TLB misses, the TLB-Refill handler, and
 * management of the swap pool.
 *
 * Policy Decisions:
 * - Page Replacement: The module implements a modified FIFO algorithm, it checks
 *   if there are any unoccupied frames first, otherwise it chooses the next frame
 * - Process Termination: When a process terminates, all its physical frames and
 *   swap entries are immediately reclaimed for use by other processes
 *
 * Functions:
 * - pager: Handles TLB miss exceptions by loading pages into memory
 * - uTLB_RefillHandler: Low-level handler for TLB refill events
 * - initSupportStructFreeList: Initializes support structures for processes
 * - allocateSupportStruct: Allocates a support structure for a new process
 * - initSwapPool: Initializes the swap pool data structure
 * - terminateUProcess: Cleans up resources when a user process terminates
 * - setInterrupts: Enables/disables interrupts for critical sections
 * - resumeState: Resumes execution of a process from a saved state
 * - clearSwapPoolEntries: Clears swap pool entries for a given ASID
 * - allocateSupportStruct: Allocates a support structure from the free list
 * - deallocateSupportStruct: Returns a support structure to the free list
 * - initSupportStructFreeList: Initializes the support structure free list
 *
 * Written by Aryah Rao & Anish Reddy
 *
 *****************************************************************************/

#include "../h/vmSupport.h"

/*----------------------------------------------------------------------------*/
/* Foward Declarations for External Functions */
/*----------------------------------------------------------------------------*/
/* sysSupport.c */
extern support_PTR getCurrentSupportStruct();
/* scheduler.c */
extern void loadProcessState(state_PTR state, unsigned int quantum);

/*----------------------------------------------------------------------------*/
/* Module variables */
/*----------------------------------------------------------------------------*/
HIDDEN support_t supportStructures[MAXUPROC+1]; /* Static array of support structures */
HIDDEN support_PTR supportFreeList = NULL;      /* Head of the support structure free list */
HIDDEN swapPoolEntry_t swapPool[SWAPPOOLSIZE];  /* Swap Pool data structure */
HIDDEN int swapPoolMutex;                       /* Semaphore for Swap Pool access */
HIDDEN int nextFrameNum;                        /* Next integer for FIFO replacement */

/*----------------------------------------------------------------------------*/
/* Helper Function Prototypes */
/*----------------------------------------------------------------------------*/
HIDDEN void deallocateSupportStruct(support_PTR supportStruct);
HIDDEN void resetSupportStruct(support_PTR supportStruct);
HIDDEN int updateFrameNum();
HIDDEN int backingStoreRW(int operation, int frameNum, int processASID, int pageNum);
HIDDEN void clearSwapPoolEntries(int asid);
HIDDEN void updateTLB(int victimNum);

/*----------------------------------------------------------------------------*/
/* Global Function Implementations */
/*----------------------------------------------------------------------------*/

/******************************************************************************
 *
 * Function: initSupportStructFreeList
 *
 * Description: Initializes the free list of support structures. Each support structure
 *              from the static array is reset and added to the free list.
 *
 * Parameters:
 *              None
 *
 * Returns:
 *              None
 *
 *****************************************************************************/
void initSupportStructFreeList() {
    supportFreeList = NULL;  /* Initialize the free list as empty */
    
    /* Add all support structures to the free list */
    int i;
    for (i = 0; i < MAXUPROC; i++) {
        deallocateSupportStruct(&supportStructures[i]);
    }
}


/******************************************************************************
 *
 * Function: allocateSupportStruct
 *
 * Description: Allocates a support structure from the free list. 
 *              If the free list is empty, returns NULL.
 *
 * Parameters:
 *              None
 *
 * Returns:
 *              Pointer to allocated support structure, or NULL
 *
 *****************************************************************************/
support_PTR allocateSupportStruct() {
    if (supportFreeList == NULL)
        return NULL;  /* No free support structures */
    
    /* Remove the first support structure from free list */
    support_PTR allocatedSupportStruct = supportFreeList;
    supportFreeList = allocatedSupportStruct->sup_next;
    
    /* Reset the support structure */
    resetSupportStruct(allocatedSupportStruct);
    
    return allocatedSupportStruct;
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
void initSwapPool() {
    int i;
    for (i = 0; i < SWAPPOOLSIZE; i++) {
        swapPool[i].asid = UNOCCUPIED;
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
 * Function: pager
 *
 * Description: Determines the cause of the fault, allocates physical frames
 *               and brings pages from backing store to fix page faults
 *
 * Parameters:
 *              None
 *
 * Returns:
 *              None
 * ======================================================================== */
void pager() {
    /* Get current process's support structure */
    support_PTR currentProcessSupport = (support_PTR)SYSCALL(GETSUPPORTPTR, 0, 0, 0);

    /* Get exception state */
    state_PTR exceptionState = &currentProcessSupport->sup_exceptState[PGFAULTEXCEPT];

    /* Why are we here? */
    int cause = (exceptionState->s_cause & CAUSE_EXCCODE_MASK) >> CAUSE_EXCCODE_SHIFT;

    /* If TLB-Modification */
    if (cause == TLBMOD) {
        terminateUProcess(NULL); /* NUKE IT! */
    }

    /* Gain swap pool mutual exclusion */
    SYSCALL(PASSEREN, (int)&swapPoolMutex, 0, 0);

    /* Pick the next frame number */
    int frameNum = updateFrameNum();

    /* Get frame address */
    memaddr frameAddress = FRAMETOADDR(frameNum);

    int status;
    int processASID = currentProcessSupport->sup_asid;
    /* If frame number is occupied */
    if ((swapPool[frameNum].asid != UNOCCUPIED)){ /* Assume page is dirty */
        /* Update that U-proc's page table & TLB atomically */
        setInterrupts(OFF);
        swapPool[frameNum].pte->pte_entryLO &= ~VALIDON;
        updateTLB(frameNum);
        setInterrupts(ON);

        /* Write the page to backing store */
        status = backingStoreRW(WRITE, frameNum, swapPool[frameNum].asid, swapPool[frameNum].vpn);
        if (status != READY) {
            terminateUProcess(&swapPoolMutex);
            return;
        }
    }
    /* Get page number */
    int pageNum = 31;
    if ((KUSEG <= exceptionState->s_entryHI) && (exceptionState->s_entryHI < PAGESTACK)) {
        pageNum = ((exceptionState->s_entryHI) & VPNMASK) >> VPNSHIFT;
    }

    /* Read the page from backing store */
    status = backingStoreRW(READ, frameNum, processASID, pageNum);
    if (status != READY) {
        terminateUProcess(&swapPoolMutex);
        return;
    }

    /* Update the swap pool entry */
    swapPool[frameNum].asid = processASID;
    swapPool[frameNum].vpn = pageNum;
    swapPool[frameNum].valid = TRUE;

    /* Update this U-proc's page table and TLB atomically */
    setInterrupts(OFF);
    swapPool[frameNum].pte = &currentProcessSupport->sup_pageTable[pageNum];
    swapPool[frameNum].pte->pte_entryLO = frameAddress | VALIDON | DIRTYON;
    updateTLB(frameNum);
    setInterrupts(ON);

    /* Release swap pool mutual exclusion */
    SYSCALL(VERHOGEN, (int)&swapPoolMutex, 0, 0);

    /* Retry the instruction */
    resumeState(exceptionState);
}


/* ========================================================================
 * Function: uTLB_RefillHandler
 *
 * Description: Refills TLB from the page table in physical memory
 *
 * Parameters:
 *              None
 *
 * Returns:
 *              None
 * ======================================================================== */
void uTLB_RefillHandler() {
    state_PTR exceptionState = (state_PTR)BIOSDATAPAGE;

    /* Get page number */
    int pageNum = 31;
    if ((KUSEG <= exceptionState->s_entryHI) && (exceptionState->s_entryHI < PAGESTACK)) {
        pageNum = ((exceptionState->s_entryHI) & VPNMASK) >> VPNSHIFT;
    }
    
    /* Update the page table entry into the TLB */
    setENTRYHI(currentProcess->p_supportStruct->sup_pageTable[pageNum].pte_entryHI);
    setENTRYLO(currentProcess->p_supportStruct->sup_pageTable[pageNum].pte_entryLO);

    /* Write the TLB in a random location */
    TLBWR();

    /* Restore the processor state */
    resumeState(exceptionState);
}


/* ========================================================================
 * Function: terminateUProcess
 *
 * Description: Terminates the current user process with proper cleanup.
 *              Releases any held mutexes, frees the support structure,
 *               clears swap pool and updates the master semaphore.
 *
 * Parameters:
 *              mutex - Semaphore that needs to be released (or NULL)
 *
 * Returns:
 *              None
 * ======================================================================== */
extern void terminateUProcess(int *mutex) {
    /* Get the current support structure */
    support_PTR supportStruct = getCurrentSupportStruct();
    
    /* Clear the current process's pages in swap pool */
    if (supportStruct != NULL) {
        clearSwapPoolEntries(supportStruct->sup_asid);
        
        /* Free the support structure */
        deallocateSupportStruct(supportStruct);
    }

    /* Release mutex if held */
    if (mutex != NULL) {
        SYSCALL(VERHOGEN, (int) mutex, 0, 0);
    }

    /* Update master semaphore to indicate process termination */
    SYSCALL(VERHOGEN, (int)&masterSema4, 0, 0);

    /* Terminate the process */
    SYSCALL(TERMINATEPROCESS, 0, 0, 0);
}


/* =======================================================================
 * Function: setInterrupts
 *
 * Description: Enables or disables interrupts
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
        status &= (~STATUS_IEc);
    }
    /* Set the status register */
    setSTATUS(status);
}


/* ========================================================================
 * Function: resumeState
 * 
 * Description: Resumes execution with a given processor state
 * 
 * Parameters:
 *              state - Processor state to load
 * 
 * Returns:
 *              None (transfers control to loaded state)
 * ======================================================================== */
void resumeState(state_t *state) {
    /* Load the processor state */
    LDST(state);
}

/*----------------------------------------------------------------------------*/
/* Helper Function Implementations */
/*----------------------------------------------------------------------------*/

/******************************************************************************
 *
 * Function: deallocateSupportStruct
 *
 * Description: Adds a support structure structure to the free list
 *
 * Parameters:
 *              supportStruct - Pointer to the support structure to be freed
 *
 * Returns:
 *              None
 *
 *****************************************************************************/
void deallocateSupportStruct(support_PTR supportStruct) {
    if (supportStruct == NULL)
        return; /* No free support structures */
    
    /* Add to free list (insert at front) */
    supportStruct->sup_next = supportFreeList;
    supportFreeList = supportStruct;
}


/******************************************************************************
 *
 * Function: resetSupportStruct
 *
 * Description: Resets a support structure to its initial values
 *
 * Parameters:
 *              supportStruct - Pointer to the support structure to reset
 *
 * Returns:
 *              None
 *
 *****************************************************************************/
void resetSupportStruct(support_PTR supportStruct) {
    /* Return if no support structure */
    if (supportStruct == NULL)
        return;
    
    supportStruct->sup_asid = UNOCCUPIED; /* Reset ASID */
    
    /* Reset exception states */
    supportStruct->sup_exceptContext[PGFAULTEXCEPT].c_pc = 0;
    supportStruct->sup_exceptContext[PGFAULTEXCEPT].c_status = 0;
    supportStruct->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = 0;
    supportStruct->sup_exceptContext[GENERALEXCEPT].c_pc = 0;
    supportStruct->sup_exceptContext[GENERALEXCEPT].c_status = 0;
    supportStruct->sup_exceptContext[GENERALEXCEPT].c_stackPtr = 0;

    /* Reset page table entries */
    int i;
    for (i = 0; i < MAXPAGES; i++) {
        supportStruct->sup_pageTable[i].pte_entryHI = 0;
        supportStruct->sup_pageTable[i].pte_entryLO = 0;
    }
    supportStruct->sup_next = NULL; /* Reset next pointer in free list */
}


/* ========================================================================
 * Function: updateFrameNum
 *
 * Description: Updates the frame number for FIFO page replacement algorithm
 *              Optimization: Check if there are any unoccupied frames before
 *              getting the next frame number using FIFO
 *
 * Parameters:
 *              None
 *
 * Returns:
 *              The next frame number to be used
 * ======================================================================== */
int updateFrameNum() {
    /* Check if there is an empty frame */
    int i;
    for (i = 0; i < SWAPPOOLSIZE; i++) {
        if (swapPool[i].asid == UNOCCUPIED) {
            nextFrameNum = i;
            return nextFrameNum;
        }
    }
    /* Update the FIFO index for next time */
    nextFrameNum = (nextFrameNum + 1) % SWAPPOOLSIZE;
    return nextFrameNum;
}


/* ========================================================================
 * Function: backingStoreRW
 *
 * Description: Performs read or write operations on flash device backing store
 *
 * Parameters:
 *              operation - The operation to perform (READ or WRITE)
 *              frameNum - The frame number to operate on
 *              processASID - The asid of which backing store to operate on
 *              pageNum - The page number to operate on
 *
 * Returns:
 *              SUCCESS if operation completed successfully
 *              ERROR if there was a problem
 * ======================================================================== */
int backingStoreRW(int operation, int frameNum, int processASID, int pageNum) {
    /* Get frame address & flash device number */
    int frameAddress = FRAMETOADDR(frameNum);
    int flashNum = processASID - 1;

    /* Calculate device semaphore index based on line and device number */
    int devMutex = ((FLASHINT - MAPINT) * DEV_PER_LINE) + (flashNum);

    /* Gain device mutex for the flash device */
    SYSCALL(PASSEREN, (int)&deviceMutex[devMutex], 0, 0);

    /* Memory address */
    devregarea_t *devRegisterArea = (devregarea_t *)RAMBASEADDR;
    devRegisterArea->devreg[devMutex].d_data0 = frameAddress; 

    /* Read a page from the flash device */
    setInterrupts(OFF);
    devRegisterArea->devreg[devMutex].d_command = (pageNum << 8) | operation;
    int status = SYSCALL(WAITIO, FLASHINT, flashNum, FALSE);
    setInterrupts(ON);

    /* Release device mutex for the flash device */
    SYSCALL(VERHOGEN, (int)&deviceMutex[devMutex], 0, 0);

    /* Return the status of the operation */
    return status;
}


/* ========================================================================
 * Function: clearSwapPoolEntries
 *
 * Description: Clears all swap pool entries belonging to the process with
 *              the specified ASID
 * 
 * Parameters:
 *              asid - The ASID of the process whose entries should be cleared
 * 
 * Returns:
 *              None
 * ======================================================================== */
void clearSwapPoolEntries(int asid) {
    /* Gain swap pool mutual exclusion */
    SYSCALL(PASSEREN, (int)&swapPoolMutex, 0, 0);
    setInterrupts(OFF);
    /* Clear all swap pool entries for the current process */
    int i;
    for (i = 0; i < SWAPPOOLSIZE; i++) {
        if (swapPool[i].asid == asid){
            swapPool[i].asid = UNOCCUPIED;
            swapPool[i].vpn = FALSE;
            swapPool[i].valid = FALSE;
            swapPool[i].pte = NULL;
        }
    }
    /* Release swap pool mutual exclusion */
    setInterrupts(ON);
    SYSCALL(VERHOGEN, (int)&swapPoolMutex, 0, 0);
}


/******************************************************************************
 *
 * Function: updateTLB
 *
 * Description: Updates the TLB entry if present
 *
 * Parameters:
 *              frameNum - Frame number whose
 *
 * Returns:
 *              None
 *
 *****************************************************************************/
void updateTLB(int frameNum){
    /* Set the EntryHi register with the VPN and ASID */
    pageTableEntry_PTR pageTableEntry = swapPool[frameNum].pte;
    setENTRYHI(pageTableEntry->pte_entryHI);
    TLBP(); /* Probe TLB */

    /* Check if matching entry was found */
    unsigned int index = getINDEX();
    if (!((index >> PROBESHIFT) & ON)) {
        /* Set EntryHi and EntryLo */
        setENTRYHI(pageTableEntry->pte_entryHI);
        setENTRYLO(pageTableEntry->pte_entryLO);

        TLBWI(); /* Update TLB */
    } /* If match not found, we'll end up in uTLB_RefillHandler */
}
