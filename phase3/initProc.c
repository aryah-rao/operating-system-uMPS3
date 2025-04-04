/*
* initProc.c - Implementation of the Process Initialization and Test module (Level 4 / Phase 3).
*
* This module contains the InstantiatorProcess (formerly the 'test' function from Level 3)
* which is responsible for initializing the Support Level environment, creating and
* launching user processes (U-procs), and potentially performing initial system tests.
* It also exports global variables used by the Support Level.
*
* Authors: Aryah Rao & Anish Reddy
* Date: Current Date
*
* Note: This module is intended to be executed in kernel mode with interrupts enabled.
*/

#include "../h/initProc.h"

/* Global variables for the Support Level */

extern int masterSema4;                             /* Master semaphore for synchronization */
extern int deviceMutex[DEVICE_COUNT];               /* Semaphores for device synchronization */
extern support_t supportStructures[MAXUPROC+1];     /* Static array of Support Structures - index 0 is reserved/sentinel */
extern swapPoolEntry_t swapPool[SWAPPOOLSIZE];      /* Swap Pool data structure */
extern int swapPoolMutex;                           /* Semaphore for Swap Pool access */

/* External declarations for handler functions from sysSupport.c */
extern void genExceptionHandler();

/* External declaration for TLB Refill Handler from vmSupport.c */
extern void pager();
extern void uTLB_RefillHandler();
extern void initSwapPool();

/* Forward declarations for helper functions */
HIDDEN void initSupportStructures();
HIDDEN int createUProc(int processIndex);


/* Test function */
void test() {
    /*
    * This function is the entry point for the Support Level testing and U-proc instantiation.
    * It runs in kernel mode with interrupts enabled.
    */

    /* Initialize support structures */
    initSupportStructures();

    /* Initialize device semaphores */
    for (int i = 0; i < DEVICE_COUNT; i++) {
        deviceMutex[i] = 1;
    }

    /* Initialize Swap Pool */
    initSwapPool();

    /* Initialize the master semaphore for synchronization */
    masterSema4 = 0;

    /* Initialize the Swap Pool data structure */

    
    /* Loop to create and launch each U-proc */
    for (int i = 1; i <= MAXUPROC; i++) {
        /* Create U-process - process index i corresponds directly to ASID i */
        if (createUProc(i) != SUCCESS) {
            /* Error handling for failed U-proc creation */
            PANIC();
            return;
        }
    }

    /* After all U-procs have been created, wait on the master semaphore, then terminate the test process */
    if (masterSema4 >= 0) {
        SYSCALL(PASSEREN, &masterSema4, 0, 0); /* Wait on the master semaphore to synchronize termination */
    } else {
        SYSCALL(TERMINATEPROCESS, 0, 0, 0); /* Terminate the test process */
    }

    /* Should never get here */
    PANIC();
}


/* HIDDEN function to initialize support structures */
HIDDEN void initSupportStructures() {
    /*
    * Initialize all support structures
    */
    for (int i = 1; i <= MAXUPROC; i++) {
        /* Initialize ASID for support structure */
        supportStructures[i].sup_asid = i;
        
        /* Initialize page table entries to invalid */
        for (int j = 0; j < MAXPAGES; j++) {    /* MAXPAGES = 2*MAXUPROC */
            supportStructures[i].sup_pageTable[j].pte_entryHI = (j << VPNSHIFT) | 0x80000000; /* VPN in proper position with KUSEG */
            supportStructures[i].sup_pageTable[j].pte_entryLO = 0; /* Not valid */
        }
    }
}


/* Helper function to create a U-process */
HIDDEN int createUProc(int processIndex) {
    /* Get the Support Structure for this processIndex */
    support_PTR newSupport = &supportStructures[processIndex];
    
    /* Set up the exception state vectors for pass-up exceptions */
    /* This will be used by the nucleus's passUpOrDie */
    
    /* For PGFAULTEXCEPT */
    newSupport->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr)pager;
    newSupport->sup_exceptContext[PGFAULTEXCEPT].c_status = ALLOFF | STATUS_IEc | STATUS_KUc | STATUS_TE;
    newSupport->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = RAMTOP - (processIndex * 3 + 1) * PAGESIZE;
    
    /* For GENERALEXCEPT */
    newSupport->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr)genExceptionHandler;
    newSupport->sup_exceptContext[GENERALEXCEPT].c_status = ALLOFF | STATUS_IEc | STATUS_KUc | STATUS_TE;
    newSupport->sup_exceptContext[GENERALEXCEPT].c_stackPtr = RAMTOP - (processIndex * 3 + 2) * PAGESIZE;

    /* Initialize the initial processor state for the U-proc */
    state_PTR initialState;
    
    /* Set initial PC and stack pointer */
    initialState->s_pc = 0x800000B0; /* Standard .text section entry */
    initialState->s_t9 = 0x800000B0; /* t9 should match PC for position-independent code */
    initialState->s_sp = 0xBFFFF000; /* Near top of KUSEG user stack area */

    /* Set up status register for user mode, interrupts enabled */
    initialState->s_status = ALLOFF;
    initialState->s_status |= STATUS_IEc; /* Enable interrupts */
    initialState->s_status |= STATUS_KUc; /* User mode */
    initialState->s_status |= STATUS_TE;  /* Timer enabled */
    initialState->s_status &= ~(STATUS_VMp | STATUS_KUp); /* Previous VM & KU cleared */
    initialState->s_status |= (1 << 28);  /* Enable Coprocessor 0 */

    /* Initialize other registers to 0 */
    for (int r = 0; r < STATEREGNUM; r++) {
        initialState->s_reg[r] = 0;
    }

    /* Create the U-proc with the initial state and the new support structure */
    int retValue = SYSCALL(CREATEPROCESS, initialState, newSupport, 0);

    /* Error handling for failed U-proc creation */
    if (retValue == ERROR) {
        return ERROR;
    }

    /* Successfully created the U-proc */
    return SUCCESS;
}
