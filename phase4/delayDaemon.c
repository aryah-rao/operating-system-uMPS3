/******************************* delayaDemon.c ********************************
 *
 * Module: Delay Daemon
 *
 * Implements the delay facility for user processes using an
 * Active Delay List (ADL) and a Delay Daemon process. The ADL is a sorted
 * singly linked list of delay descriptors, each representing a sleeping
 * U-proc and its wakeup time. The Delay Daemon wakes up sleeping U-procs
 * at the appropriate time.
 *
 * Functions:
 *   - initADL: Initializes the ADL and launches the Delay Daemon
 *   - delaySyscallHandler: Handles the SYS18 (Delay) system call
 *   - delayDaemon: The Delay Daemon process
 *   - findDelayd: Find the delay descriptor for a given wake time
 *   - allocDelayd: Allocate a delay descriptor from the free list
 *   - freeDelayd: Return a delay descriptor to the free list
 *   - insertADL: Insert a delay descriptor into the ADL in sorted order
 *   - removeExpiredADL: Remove expired descriptors from the ADL, wake up
 *                       processes, and free descriptors
 *
 * Written by Aryah Rao & Anish Reddy
 *
 *****************************************************************************/

#include "../h/delayDaemon.h"

/*----------------------------------------------------------------------------*/
/* Hidden Global Variables */
/*----------------------------------------------------------------------------*/
HIDDEN delayd_t delaydTable[MAXUPROC + 2];  /* Static array of delay descriptors (+2 for two sentinel nodes) */
HIDDEN delayd_PTR adl_h;                    /* Head sentinel for ADL */
HIDDEN delayd_PTR adl_t;                    /* Tail sentinel for ADL */
HIDDEN delayd_PTR delaydFree_h;             /* Head of free list */
HIDDEN int adlMutex;                        /* ADL mutual exclusion semaphore */

/*----------------------------------------------------------------------------*/
/* Helper Function Prototypes */
/*----------------------------------------------------------------------------*/
HIDDEN void delayDaemon();
HIDDEN delayd_PTR findDelayd(int wakeTime, delayd_PTR *prev);
HIDDEN delayd_PTR allocDelayd();
HIDDEN void freeDelayd(delayd_PTR node);
HIDDEN void insertADL(delayd_PTR node);
HIDDEN void removeExpiredADL(cpu_t currTime);

/*----------------------------------------------------------------------------*/
/* Global Function Implementations */
/*----------------------------------------------------------------------------*/

/* ========================================================================
 * Function: initADL
 *
 * Description: Initializes the ADL and launches the Delay Daemon process
 *              Sets up the two sentinels, free list, and ADL semaphore
 *
 * Parameters:
 *              None
 *
 * Returns:
 *              None
 * 
 * ======================================================================== */
void initADL() {
    /* Set up head and tail sentinels */
    adl_h = &delaydTable[0];
    adl_t = &delaydTable[1];
    adl_h->d_wakeTime = 0;           /* Smallest possible wake time */
    adl_t->d_wakeTime = MAXINT;      /* Largest possible wake time */
    adl_h->d_next = adl_t;
    adl_t->d_next = NULL;

    /* Initialize free list with the rest of the descriptors */
    delaydFree_h = NULL;
    int i;
    for (i = 2; i < (MAXUPROC + 2); ++i) {
        delaydTable[i].d_next = delaydFree_h;
        delaydFree_h = &delaydTable[i];
    }
    adlMutex = 1; /* Initialize ADL mutex */

    /* Launch Delay Daemon */
    state_t daemonState;
    daemonState.s_pc = (memaddr)delayDaemon;
    daemonState.s_t9 = (memaddr)delayDaemon;
    daemonState.s_sp = DAEMON_STACK; /* Pick a safe stack */
    daemonState.s_status = ALLOFF | STATUS_IEc | STATUS_TE; /* Kernel, interrupts on */ 
    daemonState.s_entryHI = 0; /* ASID 0 */
    /* Create Delay Daemon process */
    SYSCALL(CREATEPROCESS, (int)&daemonState, 0, 0);
}

/* ========================================================================
 * Function: delaySyscallHandler
 *
 * Description: Handles the SYS18 (Delay) system call: Puts the calling
 *              U-proc to sleep for the requested number of seconds
 *
 * Parameters:
 *              supportStruct - Pointer to the support structure of the calling process
 *
 * Returns:
 *              None
 * 
 * ======================================================================== */
void delaySyscallHandler(support_PTR supportStruct) {
    /* Validate number of seconds */
    int seconds = supportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    if (seconds < 0) {
        terminateUProcess(NULL); /* Terminate process */
        return;
    }

    /* Gain ADL mutual exclusion */
    SYSCALL(PASSEREN, (int)&adlMutex, 0, 0);

    /* Allocate delay descriptor from free list */
    delayd_PTR delayd_node = allocDelayd();
    if (!delayd_node) { /* Free list is empty */
        terminateUProcess(&adlMutex); /* Terminate process */
        return;
    }

    /* Initialize delay descriptor */
    cpu_t currTime;
    STCK(currTime);
    delayd_node->d_wakeTime = currTime + (seconds * 1000000);
    delayd_node->d_supStruct = supportStruct;
    
    /* Insert into ADL */
    insertADL(delayd_node);

    /* Atomically release ADL mutex and block on private semaphore */
    setInterrupts(OFF);
    SYSCALL(VERHOGEN, (int)&adlMutex, 0, 0);
    SYSCALL(PASSEREN, (int)&(delayd_node->d_supStruct->sup_privateSem), 0, 0);
    setInterrupts(ON);
}

/* ========================================================================
 * Function: delayDaemon
 *
 * Description: The Delay Daemon process. Waits for pseudo-clock and then
 *              wakes up any U-procs whose delay has expired
 *
 * Parameters:
 *              None
 *
 * Returns:
 *              None
 * 
 * ======================================================================== */
HIDDEN void delayDaemon() {
    while (TRUE) {
        /* Wait for pseudo-clock */
        SYSCALL(WAITCLOCK, 0, 0, 0);

        /* Gain ADL mutual exclusion */
        SYSCALL(PASSEREN, (int)&adlMutex, 0, 0);

        /* Remove expired ADL entries */
        cpu_t currTime;
        STCK(currTime);
        removeExpiredADL(currTime);

        /* Release ADL mutual exclusion */
        SYSCALL(VERHOGEN, (int)&adlMutex, 0, 0);
    }
}

/*----------------------------------------------------------------------------*/
/* Helper Function Implementations */
/*----------------------------------------------------------------------------*/

/* ========================================================================
 * Function: findDelayd
 *
 * Description: Finds the delay descriptor with the smallest d_wakeTime
 *              greater than or equal to the given wakeTime
 *              Maintains a pointer to the previous node to assist insertion
 *
 * Parameters:
 *              wakeTime - The wake time to search for
 *              prev - Pointer to store previous node
 *
 * Returns:
 *              Pointer to the found delay descriptor, or tail sentinel if not found
 * 
 * ======================================================================== */
HIDDEN delayd_PTR findDelayd(int wakeTime, delayd_PTR *prev) {
    delayd_PTR curr = adl_h->d_next; /* Start after head sentinel */
    *prev = adl_h;
    while (curr->d_wakeTime < wakeTime) { /* Sorted ascending by d_wakeTime */
        *prev = curr;
        curr = curr->d_next;
    }
    return curr;
}

/* ========================================================================
 * Function: allocDelayd
 *
 * Description: Allocates a delay descriptor from the free list
 * 
 * Parameters:
 *              None
 *
 * Returns:
 *              Pointer to allocated delayd_t, or NULL if free list is empty
 * 
 * ======================================================================== */
HIDDEN delayd_PTR allocDelayd() {
    if (!delaydFree_h) return NULL;
    delayd_PTR node = delaydFree_h;
    delaydFree_h = delaydFree_h->d_next;
    node->d_next = NULL;
    return node;
}

/* ========================================================================
 * Function: freeDelayd
 *
 * Description: Returns a delay descriptor to the free list
 *
 * Parameters:
 *              node - Pointer to the delay descriptor to free
 * 
 * Returns:
 *              None
 * 
 * ======================================================================== */
HIDDEN void freeDelayd(delayd_PTR node) {
    node->d_next = delaydFree_h;
    delaydFree_h = node;
}

/* ========================================================================
 * Function: insertADL
 *
 * Description: Inserts a delay descriptor into the ADL in ascending order
 *              of wake time
 *
 * Parameters:
 *              node - Pointer to the delay descriptor to insert
 * 
 * Returns:
 *              None
 * 
 * ======================================================================== */
HIDDEN void insertADL(delayd_PTR node) {
    delayd_PTR prev;
    delayd_PTR curr = findDelayd(node->d_wakeTime, &prev);
    node->d_next = curr;
    prev->d_next = node;
}

/* ========================================================================
 * Function: removeExpiredADL
 *
 * Description: Removes all delay descriptors from the ADL whose wake time
 *              is less than or equal to the current time, wakes up the
 *              corresponding processes (if they still exist), and frees
 *              the descriptors
 *
 * Parameters:
 *              currTime - The current time
 *
 * Returns:
 *              None
 * 
 * ======================================================================== */
HIDDEN void removeExpiredADL(cpu_t currTime) {
    delayd_PTR prev = adl_h;
    delayd_PTR curr = adl_h->d_next;

    while (curr != adl_t && curr->d_wakeTime <= currTime) {
        delayd_PTR next = curr->d_next;
        /* Check if the process still exists before waking it up */
        if (curr->d_supStruct != NULL && curr->d_supStruct->sup_asid != UNOCCUPIED) {
            /* Wake up the sleeping process */
            SYSCALL(VERHOGEN, (int)&(curr->d_supStruct->sup_privateSem), 0, 0);
        }
        /* Remove from ADL and free descriptor */
        prev->d_next = next;
        freeDelayd(curr);
        curr = next;
    }
}