/******************************* ASL.c *************************************
*
* Module: Active Semaphore List (ASL)
*
* Written by Aryah Rao & Anish Reddy
*
* Global Functions:
*
* insertBlocked             -   Insert a PCB into the ASL
* removeBlocked             -   Remove the first PCB from the ASL
* outBlocked                -   Remove a specific PCB from the ASL
* headBlocked               -   Get the head of the process queue of a semaphore
* initASL                   -   Initialize the ASL
*
* Helpers:
*
* findSemd                  -   Find the semaphore descriptor for the given semAdd
* dropFromSemaphoreQueue    -   Remove a PCB from the process queue of a semaphore
*
* The ASL is implemented as a sorted doubly linked list.
* It is sorted in descending order of semAdd.
* We implement the methods for ASL in this module.
* We use two sentinel nodes at the head and tail of the list.
* The free list of semaphore descriptors is implemented as a circular DLL.
* We use methods from the Process Control Block (PCB) module.
* The process queue of a semaphore is implemented as a circular DLL.
* We use methods from the Process Control Block (PCB) module.
*
*****************************************************************************/


#include "../h/pcb.h"
#include "../h/asl.h"


/******************** Hidden Helper Function Declarations *********************/


HIDDEN semd_PTR findSemd(int *semAdd, semd_PTR *prev); /* Find the semaphore descriptor */
HIDDEN pcb_PTR dropFromSemaphoreQueue(semd_PTR semd, semd_PTR prev, pcb_PTR p); /* Remove a PCB from the process queue of a semaphore */


/******************** Hidden Global Variables **********************/


HIDDEN semd_t semdTable[MAXPROC + 2];   /* HIDDEN array of semaphore descriptors (Adding 2 for sentinels)*/
HIDDEN semd_PTR semd_h;                 /* Head of the Active Semaphore List (ASL) */
HIDDEN semd_PTR semdFree_h;             /* Head of the free semaphore descriptor list */


/******************** ASL Global Functions ********************/


/* ========================================================================
 * Function: insertBlocked
 *
 * Description: Inserts a PCB into the process queue of a semaphore.
 * Maintains sorted order in ASL.
 *
 * Parameters:
 *               semAdd - Address of the semaphore
 *               p - Pointer to the PCB
 * 
 * Returns:
 *               FALSE if successful, TRUE if no free semaphores
 * ======================================================================== */
int insertBlocked(int *semAdd, pcb_PTR p) {
    /* Check if the process is NULL */
    if (p == NULL) {
        return TRUE;
    }

    /* Find the semaphore descriptor */
    semd_PTR prev;
    semd_PTR semd = findSemd(semAdd, &prev);

    /* Check if the semaphore descriptor exists in the ASL */
    if (semd == NULL) {
        /* Check if the free list is empty */
        if (semdFree_h == NULL) {
            return TRUE; /* No free semaphores */
        }
        
        /* Allocate a new semaphore descriptor from the free list */
        semd = (semd_PTR)removeProcQ((pcb_PTR*)&semdFree_h);

        /* Initialize descriptor */
        semd->s_semAdd = semAdd;
        semd->s_procQ = NULL;

        /* Insert into sorted ASL */
        semd->s_next = prev->s_next;
        semd->s_prev = prev;
        prev->s_next->s_prev = semd;
        prev->s_next = semd;
    }

    /* Insert process into semaphore queue */
    insertProcQ(&(semd->s_procQ), p);
    p->p_semAdd = semAdd;

    return FALSE;
}


/* ========================================================================
 * Function: removeBlocked
 *
 * Description: Removes the first PCB from the process queue of a semaphore.
 * 
 * Parameters:
 *               semAdd - Address of the semaphore
 * 
 * Returns:
 *               Pointer to the removed PCB
 *               NULL if the process queue is empty
 * ======================================================================== */
pcb_PTR removeBlocked(int *semAdd) {
    /* Find the semaphore descriptor */
    semd_PTR prev;
    semd_PTR semd = findSemd(semAdd, &prev);

    /* Call dropFromSemaphoreQueue to remove the first process from the queue */
    return dropFromSemaphoreQueue(semd, prev, NULL);
}


/* ========================================================================
 * Function: outBlocked
 *
 * Description: Removes a specific PCB from its semaphore queue.
 * 
 * Parameters:
 *               p - Pointer to the PCB
 * 
 * Returns:
 *               Pointer to the removed PCB
 *               NULL if the process queue is empty
 * ======================================================================== */
pcb_PTR outBlocked(pcb_PTR p) {
    /* Find the semaphore descriptor */
    semd_PTR prev;
    semd_PTR semd = findSemd(p->p_semAdd, &prev);

    /* Call dropFromSemaphoreQueue to remove the process from the queue */
    return dropFromSemaphoreQueue(semd, prev, p);
}


/* ========================================================================
 * Function: headBlocked
 *
 * Description: Returns the first PCB in the queue of a semaphore.
 *
 * Parameters:
 *               semAdd - Address of the semaphore
 * 
 * Returns:
 *               Pointer to the head of the process queue
 *               NULL if the process queue is empty
 * ======================================================================== */
pcb_PTR headBlocked(int *semAdd) {
    /* Find the semaphore descriptor */
    semd_PTR prev;
    semd_PTR semd = findSemd(semAdd, &prev);

    /* Check if the semaphore exists */
    if (semd == NULL) {
        return NULL;
    }

    /* Check if the process queue for this semaphore is empty */
    if (emptyProcQ(semd->s_procQ)) {
        return NULL;
    }

    /* Return the first process in the queue */
    return headProcQ(semd->s_procQ);
}

/* ========================================================================
 * Function: initASL
 *
 * Description: Initializes the ASL and free list.
 * Creates two sentinel nodes at the head and tail of the ASL.
 *
 * Parameters:
 *               None
 * 
 * Returns:
 *               None
 * ======================================================================== */
void initASL() {
    /* Create the head sentinel node for the ASL */
    semd_h = &semdTable[0];  
    semd_h->s_semAdd = 0;  /* Smallest possible semaphore address */

    /* Create the tail sentinel node for the ASL */
    semd_PTR tailSentinel = &semdTable[1];  
    tailSentinel->s_semAdd = MAXINT;  /* Largest possible semaphore address */

    /* Link the two sentinels together */
    semd_h->s_next = tailSentinel;  
    semd_h->s_prev = NULL;
    tailSentinel->s_next = NULL;
    tailSentinel->s_prev = semd_h;

    /* Initialize the free list*/
    semdFree_h = NULL;
    
    /* Insert the rest of the semaphore descriptors into the free list */
    int i;
    for (i = 2; i < MAXPROC + 2; i++) {
        /* Using the queue helper */
        insertProcQ((pcb_PTR*)&semdFree_h, (pcb_PTR)&semdTable[i]);
    }
}


/******************** Hidden Helper Functions **********************/


/* ========================================================================
 * Function: findSemd
 *
 * Description: Finds the semaphore descriptor for the given semAdd.
 * Maintains a pointer to the previous node to assist insertion/removal.
 *
 * Parameters:
 *               semAdd - Address of the semaphore
 *               prev - Pointer to store previous node
 * Returns:
 *               Pointer to the found semaphore descriptor, or NULL if not found
 * ======================================================================== */
HIDDEN semd_PTR findSemd(int *semAdd, semd_PTR *prev) {
    /* Check if the semaphore is NULL */
    if (semAdd == NULL) {
        return NULL;
    }

    /* Start the search from the first real semaphore (skip head sentinel) */
    semd_PTR curr = semd_h->s_next;

    /* Keep track of the previous node (start with head sentinel) */
    *prev = semd_h;

    /* Traverse the ASL while the current node's semAdd is smaller */
    while (curr->s_semAdd < semAdd) {
        /* Move forward in the list */
        *prev = curr;
        curr = curr->s_next;
    }

    /* Check if the current node's semAdd matches semAdd */
    if (curr->s_semAdd == semAdd) {
        return curr;  /* Found the semaphore */
    } else {
        return NULL;  /* Semaphore not in ASL */
    }
}


/* ========================================================================
 * Function: dropFromSemaphoreQueue
 *
 * Description: Removes a PCB from the process queue of a semaphore.
 *
 * Parameters:
 *               semd - Pointer to the semaphore descriptor
 *               prev - Pointer to the previous semaphore descriptor in ASL
 *               p - Pointer to the PCB to be removed (NULL for removeBlocked)
 *
 * Returns:
 *               Pointer to the removed PCB
 *               NULL if the process queue is empty
 * ======================================================================== */
HIDDEN pcb_PTR dropFromSemaphoreQueue(semd_PTR semd, semd_PTR prev, pcb_PTR p) {
    /* Check if the semaphore exists */
    if (semd == NULL) {
        return NULL;
    }

    /* Remove the process from the semaphore's process queue */
    pcb_PTR removed;
    if (p == NULL) {
        /* Remove the first process from the semaphore’s process queue */
        removed = removeProcQ(&(semd->s_procQ));
    } else {
        /* Remove a specific process from the semaphore’s process queue */
        removed = outProcQ(&(semd->s_procQ), p);
    }

    /* Check if the process was successfully removed */
    if (removed == NULL) {
        return NULL; /* Process not found */
    } else {
        removed->p_semAdd = NULL; /* Clear p_semAdd */
    }

    /* Check if the semaphore's process queue is now empty */
    if (emptyProcQ(semd->s_procQ)) {
        /* Remove semaphore from ASL */
        prev->s_next = semd->s_next;
        semd->s_next->s_prev = prev;

        /* Return the semaphore descriptor to the free list */
        insertProcQ((pcb_PTR*)&semdFree_h, (pcb_PTR)semd);
    }

    return removed;
}
