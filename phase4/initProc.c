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
#include "../h/delayDaemon.h"

/*----------------------------------------------------------------------------*/
/* Global variables */
/*----------------------------------------------------------------------------*/
int masterSema4;                             /* Master semaphore for synchronization */
int deviceMutex[DEVICE_COUNT];               /* Semaphores for device mutual exclusion */

/*----------------------------------------------------------------------------*/
/* Helper Function Prototypes */
/*----------------------------------------------------------------------------*/
HIDDEN int createUProcess(int processASID);
HIDDEN int copyImageToBackingStore(int processASID, support_PTR newSupport);

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
    int i; /* Initialize device semaphores */
    for (i = 0; i < DEVICE_COUNT; i++) {
        deviceMutex[i] = 1;
    }
    initADL(); /* Initialize the Active Delay List */
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
 * Description: Creates a new user process with the given processASID
 *              Sets up initial processor state, support structure, and
 *              copies initial image to backing store (DISK0).
 *
 * Parameters:
 *              processASID - ID of the process to create (1 to MAXUPROC)
 *
 * Returns:
 *              SUCCESS if user process was created successfully
 *              ERROR otherwise
 *
 * ======================================================================== */
int createUProcess(int processASID) {
    /* Allocate a Support Structure from the free list */
    support_PTR newSupport = allocateSupportStruct();
    if (newSupport == NULL) { /* Failed to allocate support structure */
        return ERROR;
    }

    newSupport->sup_asid = processASID; /* Set the ASID for the U-proc */
    /* Initialize page table entries to invalid */
    int j;
    for (j = 0; j < MAXPAGES; j++) {
        newSupport->sup_pageTable[j].pte_entryHI = ALLOFF | ((KUSEG + (j << VPNSHIFT)) | (processASID << ASIDSHIFT));
        newSupport->sup_pageTable[j].pte_entryLO = ALLOFF | DIRTYON;
    }

    /* Update stack page entry */
    newSupport->sup_pageTable[MAXPAGES-1].pte_entryHI = ALLOFF | (UPAGESTACK | (processASID << ASIDSHIFT));

    /* For PGFAULTEXCEPT */
    newSupport->sup_exceptContext[PGFAULTEXCEPT].c_pc = (memaddr)pager;
    newSupport->sup_exceptContext[PGFAULTEXCEPT].c_status = ALLOFF | STATUS_IEc | CAUSE_IP_MASK | STATUS_TE;
    newSupport->sup_exceptContext[PGFAULTEXCEPT].c_stackPtr = (memaddr) (UPROC_TLB_STACK(processASID));

    /* For GENERALEXCEPT */
    newSupport->sup_exceptContext[GENERALEXCEPT].c_pc = (memaddr)genExceptionHandler;
    newSupport->sup_exceptContext[GENERALEXCEPT].c_status = ALLOFF | STATUS_IEc | CAUSE_IP_MASK | STATUS_TE;
    newSupport->sup_exceptContext[GENERALEXCEPT].c_stackPtr = (memaddr) (UPROC_GEN_STACK(processASID));

    /* Copy initial image from Flash to Backing Store (DISK0) */
    if (copyImageToBackingStore(processASID, newSupport) != SUCCESS) {
        return ERROR;
    }

    /* Initial processor state */
    state_t initialState;
    initialState.s_pc = UTEXTSTART;
    initialState.s_t9 = UTEXTSTART;
    initialState.s_sp = USTACKPAGE;
    initialState.s_entryHI = processASID << ASIDSHIFT;
    initialState.s_status = ALLOFF | STATUS_KUp | STATUS_IEc | CAUSE_IP_MASK | STATUS_TE;

    /* Create user process */
    return SYSCALL(CREATEPROCESS, (int)&initialState, (int)newSupport, 0);
}

/* ========================================================================
 * Function: copyImageToBackingStore
 *
 * Description: Copies the initialized .text and .data sections of a U-proc
 *              from its assigned flash device to its allocated space on DISK0
 *              using flashRW and diskRW helpers. Acquires/releases appropriate
 *              device mutexes for each operation. Reads the .aout header to
 *              determine sizes.
 *
 * Parameters:
 *              processID - The ASID of the process (1 to MAXUPROC)
 *              newSupport - Pointer to the process's support structure
  *
 * Returns:
 *              SUCCESS if copy completed successfully
 *              ERROR otherwise
* 
 * ======================================================================== */
HIDDEN int copyImageToBackingStore(int processID, support_PTR newSupport) {
    int flashNum = processID - 1;
    int diskNum = 0;
    int blockNum = 0;
    memaddr tempBuffer = DISK_DMABUFFER_ADDR(diskNum);
    int diskDevIndex = ((DISKINT - MAPINT) * DEV_PER_LINE) + (diskNum);
    int flashDevIndex = ((FLASHINT - MAPINT) * DEV_PER_LINE) + (flashNum);

    /* Gain disk0 and flash0 mutex */
    SYSCALL(PASSEREN, (int)&deviceMutex[diskDevIndex], 0, 0);
    SYSCALL(PASSEREN, (int)&deviceMutex[flashDevIndex], 0, 0);
    
    /* Read Block 0 header info */
    int status = flashRW(READ, flashNum, blockNum, tempBuffer);
    if (status != READY) {
        return ERROR;
    }

    /* Process header from block 0 */
    memaddr *header = (memaddr *)tempBuffer;
    newSupport->sup_textSize = header[3];
    unsigned int textSizeInFile = header[5];
    unsigned int dataSizeInFile = header[9];

    /* Calculate number of blocks needed */
    unsigned int totalFileSize = textSizeInFile + dataSizeInFile;
    unsigned int numBlocksToCopy = (totalFileSize + PAGESIZE - 1) / PAGESIZE;

    /* Validate number of blocks to copy */
    if (numBlocksToCopy == 0) {
        numBlocksToCopy = 1; /* Should copy at least block 0 */
    } 
    if (numBlocksToCopy >= MAXPAGES) {
        return ERROR; /* Too many blocks to copy */
    } 

    /* Write Block 0 to Disk */
    int linearSector = (processID - 1) * MAXPAGES + blockNum;
    status = diskRW(WRITEBLK, diskNum, linearSector, tempBuffer);
    if (status != READY) {
        return ERROR;
    }

    /* Loop for remaining blocks */
    blockNum = 1;
    while (blockNum < numBlocksToCopy) {
        /* Read block from Flash */
        status = flashRW(READ, flashNum, blockNum, tempBuffer);
        if (status != READY) {
            return ERROR;
        }
        /* Write block to Disk using diskRW */
        linearSector = (processID - 1) * MAXPAGES + blockNum;
        status = diskRW(WRITEBLK, diskNum, linearSector, tempBuffer);
        if (status != READY) {
            return ERROR;
        }
        blockNum++;
    }
    /* Release DISK0 and FLASH0 mutexes */
    SYSCALL(VERHOGEN, (int)&deviceMutex[diskDevIndex], 0, 0);
    SYSCALL(VERHOGEN, (int)&deviceMutex[flashDevIndex], 0, 0);
    return SUCCESS;
}
