/******************************* PCB.c *************************************
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
*
* initASL - Initialize the ASL
*
* The ASL is implemented as a sorted linked list.
* It is sorted in descending order of semAdd.
* We use a sentinel node at the head and tail of the list.
* The free list of semaphore descriptors is implemented as a circular DLL.
* The process queue of a semaphore is implemented as a circular DLL.
*
*****************************************************************************/

#include "../h/pcb.h"
#include "../h/asl.h"

/******************** Hidden Global Variables **********************/

static semd_t semdTable[MAXPROC + 2]; /* Static array of semaphore descriptors */
static semd_t *semd_h;  /* Head of the Active Semaphore List (ASL) */
static semd_t *semdFree_h; /* Head of the free semaphore descriptor list */

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
static semd_t *findSemd(int *semAdd, semd_t **prev) {
    semd_t *curr = semd_h->s_next;
    *prev = semd_h;

    while (curr->s_semAdd < semAdd) {
        *prev = curr;
        curr = curr->s_next;
    }

    return (curr->s_semAdd == semAdd) ? curr : NULL;
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
    semd_h = &semdTable[0];
    semd_h->s_semAdd = 0;
    
    semd_t *tailSentinel = &semdTable[1];
    tailSentinel->s_semAdd = MAXINT;

    /* Link sentinels */
    semd_h->s_next = tailSentinel;
    semd_h->s_prev = NULL;
    tailSentinel->s_next = NULL;
    tailSentinel->s_prev = semd_h;

    /* Initialize free list */
    semdFree_h = &semdTable[2];
    int i;
    for (i = 2; i < MAXPROC + 1; i++) {
        semdTable[i].s_next = &semdTable[i + 1];
    }
    semdTable[MAXPROC + 1].s_next = NULL;
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
int insertBlocked(int *semAdd, pcb_t *p) {
    semd_t *prev;
    semd_t *semd = findSemd(semAdd, &prev);

    if (semd == NULL) {
        if (semdFree_h == NULL) return TRUE; /* No free semaphores */
        
        /* Allocate a new semaphore descriptor */
        semd = semdFree_h;
        semdFree_h = semdFree_h->s_next;

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
pcb_t *removeBlocked(int *semAdd) {
    /* Find the semaphore descriptor associated with p->p_semAdd */
    semd_t *prev;
    semd_t *semd;
    semd = findSemd(semAdd, &prev);

    /* Check if the semaphore descriptor exists in the ASL */
    if (semd == NULL) {
        return NULL;
    }

    /* Remove the first process from the semaphore’s process queue */
    pcb_t *p = removeProcQ(&(semd->s_procQ));

    /* Check if the process queue was empty */
    if (p == NULL) {
        /* If no process was in the queue, return NULL */
        return NULL;
    }

    /* Since the process is no longer blocked on the semaphore, set its p_semAdd to NULL */
    p->p_semAdd = NULL;

    /* Check if the semaphore's process queue is now empty */
    if (emptyProcQ(semd->s_procQ)) {
        /* The process queue is empty, so we must remove this semaphore from ASL */

        /* Unlink the semaphore descriptor from the ASL */
        prev->s_next = semd->s_next;
        semd->s_next->s_prev = prev;

        /* Return the semaphore descriptor to the free list */
        semd->s_next = semdFree_h;
        semdFree_h = semd;
    }

    /* Return the removed PCB */
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
 * ======================================================================== */pcb_t *outBlocked(pcb_t *p) {
    /* Check if the process exists */
    if (p == NULL) {
        return NULL;
    }

    /* Check if the process is currently blocked on a semaphore */
    if (p->p_semAdd == NULL) {
        return NULL;
    }
    
    /* Find the semaphore descriptor associated with p->p_semAdd */
    semd_t *prev;
    semd_t *semd = findSemd(p->p_semAdd, &prev);
    
    /* Check if the semaphore was found in the ASL */
    if (semd == NULL) {
        return NULL;
    }

    /* Remove the process from the semaphore's process queue */
    pcb_t *removed = outProcQ(&(semd->s_procQ), p);

    /* Check if the process was successfully removed */
    if (removed != NULL) {
        /* Process successfully removed; update its semaphore pointer */
        removed->p_semAdd = NULL;
    } else {
        /* Process was not found in the queue, return NULL */
        return NULL;
    }
    
    /* Check if the semaphore's process queue is now empty */
    if (emptyProcQ(semd->s_procQ)) {
        /* Unlink the semaphore descriptor from the ASL */
        prev->s_next = semd->s_next;
        semd->s_next->s_prev = prev;

        /* Return the semaphore descriptor to the free list */
        semd->s_next = semdFree_h;
        semdFree_h = semd;
    }
    /* Return the removed PCB */
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
pcb_t *headBlocked(int *semAdd) {
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
