/******************************* ASL.c *************************************
*
* Module: Active Semaphore List (ASL)

*
* Implemented as a circular buffer in the integer array queue[QSIZE].
* Count is the current number of elements in the queue.
*****************************************************************************/

#include "../h/asl.h"
#include "../h/pcb.h"

#define MAXSEMD 20  /* Maximum number of semaphores in ASL */

/* Free list of semaphore descriptors */
static semd_t semdTable[MAXSEMD];
static semd_t *semdFreeList;

/* Active Semaphore List */
static semd_t *semdList;

/* =======================================================
   Function: initASL()
   Purpose: Initialize the Active Semaphore List (ASL)
   ======================================================= */
void initASL() {
    int i;
    
    /* Link all semaphores in the free list */
    for (i = 0; i < MAXSEMD - 1; i++) {
        semdTable[i].s_next = &semdTable[i + 1];
    }
    semdTable[MAXSEMD - 1].s_next = NULL;

    /* Initialize the free list and the ASL */
    semdFreeList = &semdTable[0];
    semdList = NULL;
}

/* =======================================================
   Function: insertBlocked()
   Purpose: Block a process `p` on a semaphore `semAdd`
   Returns: 0 on success, -1 if no available semaphores
   ======================================================= */
int insertBlocked(int *semAdd, pcb_PTR p) {
    semd_t *semd = semdList;
    semd_t *prev = NULL;

    /* Search for an existing semaphore */
    while (semd != NULL) {
        if (semd->s_semAdd == semAdd) {
            break;
        }
        prev = semd;
        semd = semd->s_next;
    }

    /* If semaphore not found, allocate a new descriptor */
    if (semd == NULL) {
        if (semdFreeList == NULL) {
            return -1; /* No available semaphores */
        }

        /* Allocate new semaphore from the free list */
        semd = semdFreeList;
        semdFreeList = semdFreeList->s_next;
        semd->s_semAdd = semAdd;
        semd->s_procQ = mkEmptyProcQ();

        /* Insert into the ASL */
        semd->s_next = semdList;
        semdList = semd;
    }

    /* Add the process to the semaphore queue */
    insertProcQ(&(semd->s_procQ), p);
    p->p_semAdd = semAdd;

    return 0;
}

/* =======================================================
   Function: removeBlocked()
   Purpose: Remove and return the first process from `semAdd`
   Returns: Pointer to the removed process, or NULL if empty
   ======================================================= */
pcb_PTR removeBlocked(int *semAdd) {
    semd_t *prev = NULL;
    semd_t *semd = semdList;

    /* Find the semaphore in the ASL */
    while (semd != NULL && semd->s_semAdd != semAdd) {
        prev = semd;
        semd = semd->s_next;
    }

    if (semd == NULL) return NULL; /* Semaphore not found */

    /* Remove first process from queue */
    pcb_PTR removedProc = removeProcQ(&(semd->s_procQ));
    if (removedProc != NULL) {
        removedProc->p_semAdd = NULL;
    }

    /* If queue becomes empty, remove the semaphore from ASL */
    if (emptyProcQ(semd->s_procQ)) {
        if (prev != NULL) {
            prev->s_next = semd->s_next;
        } else {
            semdList = semd->s_next;
        }

        /* Return descriptor to free list */
        semd->s_next = semdFreeList;
        semdFreeList = semd;
    }

    return removedProc;
}

/* =======================================================
   Function: outBlocked()
   Purpose: Remove a specific process `p` from `semAdd`
   Returns: Pointer to removed process, or NULL if not found
   ======================================================= */
pcb_PTR outBlocked(pcb_PTR p) {
    if (p == NULL || p->p_semAdd == NULL) return NULL;

    semd_t *semd = semdList;
    semd_t *prev = NULL;

    /* Locate the semaphore descriptor */
    while (semd != NULL) {
        if (semd->s_semAdd == p->p_semAdd) {
            break;
        }
        prev = semd;
        semd = semd->s_next;
    }

    if (semd == NULL) return NULL;

    /* Remove process from queue */
    pcb_PTR removedProc = outProcQ(&(semd->s_procQ), p);
    if (removedProc != NULL) {
        removedProc->p_semAdd = NULL;
    }

    /* If queue becomes empty, remove the semaphore from ASL */
    if (emptyProcQ(semd->s_procQ)) {
        if (prev != NULL) {
            prev->s_next = semd->s_next;
        } else {
            semdList = semd->s_next;
        }

        /* Return descriptor to free list */
        semd->s_next = semdFreeList;
        semdFreeList = semd;
    }

    return removedProc;
}

/* =======================================================
   Function: headBlocked()
   Purpose: Return the first process in the `semAdd` queue
   Returns: Pointer to first process, or NULL if queue is empty
   ======================================================= */
pcb_PTR headBlocked(int *semAdd) {
    semd_t *semd = semdList;

    /* Search for semaphore descriptor */
    while (semd != NULL) {
        if (semd->s_semAdd == semAdd) {
            return headProcQ(semd->s_procQ);
        }
        semd = semd->s_next;
    }

    return NULL;
}
