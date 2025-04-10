/******************************* initProc.c *************************************
 *
 * Module: Process Initialization and Test
 *
 * Description:
 * This module contains the InstantiatorProcess (formerly the 'test' function 
 * from Level 3) which is responsible for initializing the Support Level 
 * environment, creating and launching user processes (U-procs), and potentially
 * performing initial system tests. It also exports global variables used by
 * the Support Level.
 *
 * Implementation:
 * The module initializes support structures for each U-proc with appropriate
 * exception contexts, creates virtual memory page tables, and launches U-procs
 * with proper initialization states. It uses system calls to create processes
 * and maintains a master semaphore for synchronization of process termination.
 * 
 * Functions:
 * - test: Entry point for the Support Level initialization and U-proc creation.
 * - initSupportStructures: Initializes support structures for each U-proc.
 * - createUProc: Creates a U-process using a predefined support structure.
 * 
 * Written by Aryah Rao and Anish Reddy
 *
 ***************************************************************************/

#include "../h/initProc.h"

/* Global variables for the Support Level */

/* Define global variables that are used across support level modules */
int masterSema4;                             /* Master semaphore for synchronization */
int deviceMutex[DEVICE_COUNT];               /* Semaphores for device synchronization */
support_t supportStructures[MAXUPROC+1];     /* Static array of Support Structures - index 0 is reserved/sentinel */

/* External declarations for handler functions from sysSupport.c */
extern void genExceptionHandler();

/* External declaration for TLB Refill Handler from vmSupport.c */
extern void pager();
extern void uTLB_RefillHandler();
extern void initSwapPool();

/* Forward declarations for helper functions */
HIDDEN void initSupportStructures();
HIDDEN int createUProc(int processID);


/* ========================================================================
 * Function: test
 *
 * Description: Entry point for the Support Level testing and U-proc instantiation.
 *              Initializes system structures, creates U-procs, and waits for
 *              completion before terminating.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              None (doesn't normally return)
 * ======================================================================== */
void test() {
    /* Initialize support structures */
    initSupportStructures();

    /* Initialize device semaphores */
    int i;
    for (i = 0; i < DEVICE_COUNT; i++) {
        deviceMutex[i] = 1;
    }

    /* Initialize the Swap Pool data structure */
    initSwapPool();

    /* Initialize the master semaphore for synchronization */
    masterSema4 = 0;

    /* Loop to create and launch each U-proc */
    int j;
    for (j = 1; j <= MAXUPROC; j++) {
        /* Create U-process - process index i corresponds directly to ASID i */
        if (createUProc(j) != SUCCESS) {
            /* Terminate process if U-proc creation fails */
            SYSCALL(TERMINATEPROCESS, 0, 0, 0);
        }
    }

    /* After all U-procs have been created, wait on the master semaphore for each U-proc */
    int k;
    for (k = 1; k <= MAXUPROC; k++) {
        SYSCALL(PASSEREN, (int)&masterSema4, 0, 0); 
        /* Wait for each child to signal completion */
    }

    /* All children have terminated, now terminate the test process */
    SYSCALL(TERMINATEPROCESS, 0, 0, 0);

    /* Should never get here */
    PANIC();
}


/* ========================================================================
 * Function: initSupportStructures
 *
 * Description: Initializes all support structures for U-procs, including
 *              page tables, exception contexts, and ASID assignments.
 *              This prepares the necessary data structures for virtual memory
 *              and exception handling for each user process.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              None
 * ======================================================================== */
HIDDEN void initSupportStructures() {
    int id;

    for (id = 1; id <= MAXUPROC; id++) {
        /* Initialize ASID for support structure */
        supportStructures[id].sup_asid = id;
        
        /* Initialize page table entries to invalid */
        int j;
        for (j = 0; j < MAXPAGES; j++) {    /* MAXPAGES = 2*MAXUPROC */
            supportStructures[id].sup_pageTable[j].pte_entryHI = ALLOFF | ((UPROCSTRT + j) << VPNSHIFT) | (id << ASIDSHIFT); /* VPN in proper position with KUSEG */
            supportStructures[id].sup_pageTable[j].pte_entryLO = ALLOFF | DIRTYON; /* Not valid */
        }

        /* Stack */
        supportStructures[id].sup_pageTable[MAXPAGES-1].pte_entryHI = ALLOFF | (PAGESTACK << VPNSHIFT) | (id << ASIDSHIFT);
        
        /* Set up the exception state vectors for pass-up exceptions */
        /* This will be used by the nucleus's passUpOrDie */
        
        /* For PGFAULTEXCEPT */
        supportStructures[id].sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr)pager;
        supportStructures[id].sup_exceptContext[PGFAULTEXCEPT].c_status = ALLOFF | STATUS_IEc | CAUSE_IP_MASK | STATUS_TE;
        supportStructures[id].sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = (memaddr) &supportStructures[id].sup_stackTLB[499]; /* Stack pointer for the U-proc */
        
        /* For GENERALEXCEPT */
        supportStructures[id].sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr)genExceptionHandler;
        supportStructures[id].sup_exceptContext[GENERALEXCEPT].c_status = ALLOFF | STATUS_IEc | CAUSE_IP_MASK | STATUS_TE;
        supportStructures[id].sup_exceptContext[GENERALEXCEPT].c_stackPtr = (memaddr) &supportStructures[id].sup_stackGen[499]; /* Stack pointer for the U-proc */
    }
}


/* ========================================================================
 * Function: createUProc
 *
 * Description: Creates a new user process (U-proc) with the given processID.
 *              Sets up initial processor state for the U-proc including
 *              program counter, stack pointer, and status register values
 *              to ensure proper execution in user mode.
 * 
 * Parameters:
 *              processID - ID of the process to create (1 to MAXUPROC)
 * 
 * Returns:
 *              SUCCESS if U-proc was created successfully
 *              ERROR if creation failed
 * ======================================================================== */
HIDDEN int createUProc(int processID) {
    /* Get the Support Structure for this processID */
    support_PTR newSupport = &supportStructures[processID];
    
    /* Initialize the initial processor state for the U-proc */
    state_t initialState;
    
    /* Set initial PC and stack pointer */
    initialState.s_pc = TEXTSTART; /* Standard .text section entry */
    initialState.s_t9 = TEXTSTART; /* t9 should match PC for position-independent code */
    initialState.s_sp = USTACKPAGE; /* Near top of KUSEG user stack area */
    initialState.s_entryHI = (processID << 6) & 0x000003C0; /* Set the VPN for the U-proc, KUSEG space */

    /* Set up status register for user mode, interrupts enabled */
    initialState.s_status = ALLOFF;
    initialState.s_status |= STATUS_KUc; /* User mode */
    initialState.s_status |= STATUS_IEc; /* Enable interrupts */
    initialState.s_status |= CAUSE_IP_MASK; /* Interrupts allowed from all devices */
    initialState.s_status |= STATUS_TE;  /* Timer enabled */

    /* Successfully created the U-proc */
    return SYSCALL(CREATEPROCESS, (int)&initialState, (int)newSupport, 0);
}
