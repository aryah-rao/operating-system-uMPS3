/******************************* ASL.c *************************************
*
* Module: Active Semaphore List (ASL)
*
* Written by Aryah Rao & Anish Reddy
*
* We provide functions for the Active Semaphore List (ASL):
*
* insertBlocked - Insert a PCB into the ASL
* removeBlocked - Remove the first PCB from the ASL
* outBlocked - Remove a specific PCB from the ASL
* headBlocked - Get the head of the process queue of a semaphore
* initASL - Initialize the ASL
*
* The ASL is implemented as a sorted doubly linked list.
* It is sorted in descending order of semAdd.
* We use two sentinel nodes at the head and tail of the list.
* The free list of semaphore descriptors is implemented as a circular DLL.
* We use methods from the Process Control Block (PCB) module.
* The process queue of a semaphore is implemented as a circular DLL.
* We use methods from the Process Control Block (PCB) module.
*
*****************************************************************************/


#include "../h/pcb.h"
#include "../h/asl.h"


/******************** Hidden Global Variables **********************/


HIDDEN semd_t semdTable[MAXPROC + 2]; /* HIDDEN array of semaphore descriptors */
HIDDEN semd_t *semd_h;  /* Head of the Active Semaphore List (ASL) */
HIDDEN semd_t *semdFree_h; /* Head of the free semaphore descriptor list */


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
HIDDEN semd_t *findSemd(int *semAdd, semd_t **prev) {
    /* Start the search from the first real semaphore (skip head sentinel) */
    semd_t *curr = semd_h->s_next;

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


/******************** ASL Global Functions ********************/


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
    semd_t *tailSentinel = &semdTable[1];  
    tailSentinel->s_semAdd = MAXINT;  /* Largest possible semaphore address */

    /* Link the two sentinels together */
    semd_h->s_next = tailSentinel;  
    semd_h->s_prev = NULL;
    tailSentinel->s_next = NULL;
    tailSentinel->s_prev = semd_h;

    /* Initialize the free list using a circular DLL */
    semdFree_h = NULL;  /* Start with an empty circular DLL */
    
    /* Insert the rest of the semaphore descriptors into the free list */
    int i;
    for (i = 2; i < MAXPROC + 2; i++) {
        insertProcQ((pcb_t**)&semdFree_h, (pcb_t*)&semdTable[i]);
    }
}


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
    semd_t *prev;
    semd_t *semd = findSemd(semAdd, &prev);

    /* Check if the semaphore descriptor exists in the ASL */
    if (semd == NULL) {
        /* Check if the free list is empty */
        if (semdFree_h == NULL) {
            return TRUE; /* No free semaphores */
        }
        
        /* Allocate a new semaphore descriptor from the free list */
        semd = (semd_t *)removeProcQ((pcb_t**)&semdFree_h);

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
    /* Find the semaphore descriptor associated with p->p_semAdd */
    semd_t *prev;
    semd_t *semd;
    semd = findSemd(semAdd, &prev);

    /* Check if the semaphore descriptor exists in the ASL */
    if (semd == NULL) {
        return NULL;
    }

    /* Remove the first process from the semaphore’s process queue */
    pcb_PTR p = removeProcQ(&(semd->s_procQ));

    /* Check if the process queue was empty */
    if (p == NULL) {
        return NULL;
    }

    /* Since the process is no longer blocked on the semaphore, set its p_semAdd to NULL */
    p->p_semAdd = NULL;

    /* Check if the semaphore's process queue is now empty */
    if (emptyProcQ(semd->s_procQ)) {
        /* The process queue is empty, so we must remove this semaphore from ASL */
        prev->s_next = semd->s_next;
        semd->s_next->s_prev = prev;

        /* Return the semaphore descriptor to the free list */
        insertProcQ((pcb_t**)&semdFree_h, (pcb_t*)semd);
    }

    return p;
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
    if (p == NULL || p->p_semAdd == NULL) {
        return NULL;
    }
    
    /* Find the semaphore descriptor associated with p->p_semAdd */
    semd_t *prev;
    semd_t *semd = findSemd(p->p_semAdd, &prev);
    
    if (semd == NULL) {
        return NULL;
    }

    /* Remove the process from the semaphore's process queue */
    pcb_PTR removed = outProcQ(&(semd->s_procQ), p);

    /* Check if the process was successfully removed */
    if (removed != NULL) {
        removed->p_semAdd = NULL;
    } else {
        return NULL;
    }
    
    /* Check if the semaphore's process queue is now empty */
    if (emptyProcQ(semd->s_procQ)) {
        /* Remove from ASL */
        prev->s_next = semd->s_next;
        semd->s_next->s_prev = prev;

        /* Return the semaphore descriptor to the free list */
        insertProcQ((pcb_t**)&semdFree_h, (pcb_t*)semd);
    }
    
    return removed;
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
    semd_t *prev;
    semd_t *semd = findSemd(semAdd, &prev);

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