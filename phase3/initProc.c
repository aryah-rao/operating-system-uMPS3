/******************************* initProc.c *************************************
 *
 * Module: Initial Process
 *
 * Description:
 * This module contains the 'test' function, which is responsible for initializing
 * the Support Level environment, creating and launching user processes, and 
 * serving as the parent for all user processes in the system.
 * 
 * For each user process:
 * 1. A support structure is allocated with a unique ASID (Address Space ID)
 * 2. Page tables are initialized with all entries initially invalid
 * 3. Exception contexts are set up for both TLB management and general exceptions
 * 4. Dedicated stack space is allocated in the kernel area for handling exceptions
 * 5. The process is created with appropriate privileges (user mode)
 *
 * Process synchronization is handled through a master semaphore that tracks 
 * process termination. The test waits for all child processes to terminate before
 * terminating.
 *
 * Functions:
 * - test: Entry point for the Support Level initialization and U-proc creation
 * - createUProcess: Creates a U-process using a predefined support structure
 * 
 * Written by Aryah Rao and Anish Reddy
 *
 ***************************************************************************/

#include "../h/initProc.h"

/*----------------------------------------------------------------------------*/
/* Foward Declarations for External Functions */
/*----------------------------------------------------------------------------*/
/* sysSupport.c */
extern void genExceptionHandler();
/* vmSupport.c */
extern void initSupportStructFreeList();
extern support_PTR allocateSupportStruct();
extern void initSwapPool();
extern void pager();
extern void uTLB_RefillHandler();

/*----------------------------------------------------------------------------*/
/* Global variables */
/*----------------------------------------------------------------------------*/
int masterSema4;                             /* Master semaphore for synchronization */
int deviceMutex[DEVICE_COUNT];               /* Semaphores for device synchronization */

/*----------------------------------------------------------------------------*/
/* Helper Function Prototypes */
/*----------------------------------------------------------------------------*/
HIDDEN int createUProcess(int processID);

/*----------------------------------------------------------------------------*/
/* Global Function Implementations */
/*----------------------------------------------------------------------------*/

/* ========================================================================
 * Function: test
 *
 * Description: Initializes support structures, creates U-procs, and waits for
 *              completion before terminating
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              None
 * ======================================================================== */
void test() {
    /* Initialize support level data structures */
    
    int i; /* Initialize device semaphores */
    for (i = 0; i < DEVICE_COUNT; i++) {
        deviceMutex[i] = 1;
    }
    initSupportStructFreeList(); /* Initialize the free list of support structures */
    initSwapPool(); /* Initialize the Swap Pool data structure */
    masterSema4 = 0; /* Initialize the master semaphore for synchronization */

    /* Create user processes */
    int asid;
    for (asid = 1; asid <= MAXUPROC; asid++) {
        if (createUProcess(asid) != SUCCESS) { /* Failed to create U-proc */
            SYSCALL(TERMINATEPROCESS, 0, 0, 0); /* Nuke it! */
        }
    }

    /* After all U-procs have been created, wait on the master semaphore for each user process */
    for (asid = 1; asid <= MAXUPROC; asid++) {
        SYSCALL(PASSEREN, (int)&masterSema4, 0, 0); 
    }
    /* All children have terminated, now terminate the test process */
    SYSCALL(TERMINATEPROCESS, 0, 0, 0);
    PANIC(); /* Should never get here */
}


/* ========================================================================
 * Function: createUProcess
 *
 * Description: Creates a new user process with the given processID
 *              Sets up initial processor state and support structure
 * 
 * Parameters:
 *              processID - ID of the process to create
 * 
 * Returns:
 *              SUCCESS if user process was created successfully
 *
 * ======================================================================== */
int createUProcess(int processID) {
    /* Allocate a Support Structure from the free list */
    support_PTR newSupport = allocateSupportStruct();
    if (newSupport == NULL) { /* Failed to allocate support structure */
        return ERROR;
    }

    newSupport->sup_asid = processID; /* Set the ASID for the U-proc */
    /* Initialize page table entries to invalid */
    int j;
    for (j = 0; j < MAXPAGES; j++) {
        newSupport->sup_pageTable[j].pte_entryHI = ALLOFF | ((KUSEG + (j << VPNSHIFT)) | (processID << ASIDSHIFT));
        newSupport->sup_pageTable[j].pte_entryLO = ALLOFF | DIRTYON;
    }

    /* Update stack page */
    newSupport->sup_pageTable[MAXPAGES-1].pte_entryHI = ALLOFF | (PAGESTACK + (processID << ASIDSHIFT));

    /* For PGFAULTEXCEPT */
    newSupport->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr)pager;
    newSupport->sup_exceptContext[PGFAULTEXCEPT].c_status = ALLOFF | STATUS_IEc | CAUSE_IP_MASK | STATUS_TE;
    newSupport->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = (memaddr) ((RAMTOP - (2 * PAGESIZE * processID)) + PAGESIZE);

    /* For GENERALEXCEPT */
    newSupport->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr)genExceptionHandler;
    newSupport->sup_exceptContext[GENERALEXCEPT].c_status = ALLOFF | STATUS_IEc | CAUSE_IP_MASK | STATUS_TE;
    newSupport->sup_exceptContext[GENERALEXCEPT].c_stackPtr = (memaddr) ((RAMTOP - (2 * PAGESIZE * processID)));
    
    /* Initial processor state */
    state_t initialState;
    initialState.s_pc = TEXTSTART;
    initialState.s_t9 = TEXTSTART;
    initialState.s_sp = USTACKPAGE;
    initialState.s_entryHI = processID << ASIDSHIFT;
    initialState.s_status = ALLOFF | STATUS_KUp | STATUS_IEc | CAUSE_IP_MASK | STATUS_TE;

    /* Create user process */
    return SYSCALL(CREATEPROCESS, (int)&initialState, (int)newSupport, 0);
}
