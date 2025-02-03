/******************************* ASL.c *************************************
*
* Module: Active Semaphore List (ASL) Management
*
* Written by Aryah Rao & Anish Reddy
*
* This module provides semaphore management functions:
* - Managing a sorted doubly linked list (DLL) with sentinel nodes.
* - Associating semaphores with process queues (PCBs).
* - Efficient insertion, removal, and lookup of blocked processes.
*
* Uses:
* - Doubly linked list (with head and tail sentinels) for ASL.
* - Process queues from pcb.c to manage blocked processes.
*****************************************************************************/

#include "../h/asl.h"
#include "../h/pcb.h"


/******************** Hidden Global Variables ********************/

static semd_t semdTable[MAXPROC + 2]; /* Static array of semaphore descriptors */
static semd_t *semd_h;  /* Head of the active semaphore list */
static semd_t *semdFree_h; /* Head of the free semaphore list */

/******************** Hidden Doubly Linked List Functions ********************/

/* =======================================================
   Function: initDLL()
   Purpose: Initializes the doubly linked list with sentinel nodes.
   Parameters: None
   Returns: None
   ======================================================= */
static void initDLL() {
    semd_h = &semdTable[0];
    semd_h->s_semAdd = (int *)0;
    semd_h->s_next = &semdTable[MAXPROC + 1];
    semd_h->s_prev = NULL;
    
    semdTable[MAXPROC + 1].s_semAdd = (int *)MAXINT;
    semdTable[MAXPROC + 1].s_next = NULL;
    semdTable[MAXPROC + 1].s_prev = semd_h;
}

/* =======================================================
   Function: insertSemd()
   Purpose: Inserts a semaphore descriptor into the sorted ASL.
   Parameters: Pointer to new semaphore descriptor
   Returns: None
   ======================================================= */
static void insertSemd(semd_t *newSemd) {
    semd_t *current = semd_h;
    while (current->s_next->s_semAdd < newSemd->s_semAdd) {
        current = current->s_next;
    }
    newSemd->s_next = current->s_next;
    newSemd->s_prev = current;
    current->s_next->s_prev = newSemd;
    current->s_next = newSemd;
}

/* =======================================================
   Function: removeSemd()
   Purpose: Removes a semaphore descriptor from the ASL.
   Parameters: Semaphore address
   Returns: Pointer to removed semaphore descriptor
   ======================================================= */
static semd_t *removeSemd(int *semAdd) {
    semd_t *current = semd_h->s_next;
    while (current->s_semAdd < semAdd) {
        current = current->s_next;
    }
    if (current->s_semAdd != semAdd) return NULL;
    current->s_prev->s_next = current->s_next;
    current->s_next->s_prev = current->s_prev;
    return current;
}

/******************** ASL Management Functions ********************/

/* =======================================================
   Function: initASL()
   Purpose: Initializes the ASL with sentinel nodes and free list.
   Parameters: None
   Returns: None
   ======================================================= */
void initASL() {
    initDLL();
    semdFree_h = &semdTable[1];
    int i;
    for (i = 1; i < MAXPROC; i++) {
        semdTable[i].s_next = &semdTable[i + 1];
    }
    semdTable[MAXPROC].s_next = NULL;
}

/* =======================================================
   Function: insertBlocked()
   Purpose: Inserts a process into the ASL.
   Parameters: Semaphore address, Process to block
   Returns: 1 if no space available, 0 otherwise
   ======================================================= */
int insertBlocked(int *semAdd, pcb_t *p) {
    semd_t *current = semd_h->s_next;
    while (current->s_semAdd < semAdd) {
        current = current->s_next;
    }
    if (current->s_semAdd == semAdd) {
        insertProcQ(&(current->s_procQ), p);
        return 0;
    }
    if (semdFree_h == NULL) return 1;
    semd_t *newSemd = semdFree_h;
    semdFree_h = semdFree_h->s_next;
    newSemd->s_semAdd = semAdd;
    newSemd->s_procQ = NULL;
    insertProcQ(&(newSemd->s_procQ), p);
    insertSemd(newSemd);
    return 0;
}

/* =======================================================
   Function: removeBlocked()
   Purpose: Removes and returns the first blocked process for a given semaphore.
   Parameters: Semaphore address
   Returns: Pointer to removed process
   ======================================================= */
pcb_t *removeBlocked(int *semAdd) {
    semd_t *current = semd_h->s_next;
    while (current->s_semAdd < semAdd) {
        current = current->s_next;
    }
    if (current->s_semAdd != semAdd) return NULL;
    pcb_t *removedProc = removeProcQ(&(current->s_procQ));
    if (removedProc == NULL) {
        removeSemd(semAdd);
    }
    return removedProc;
}

/* =======================================================
   Function: outBlocked()
   Purpose: Removes a specific blocked process from the ASL.
   Parameters: Process to remove
   Returns: Pointer to removed process
   ======================================================= */
pcb_t *outBlocked(pcb_t *p) {
    if (p == NULL || p->p_semAdd == NULL) return NULL;
    semd_t *current = semd_h->s_next;
    while (current->s_semAdd < p->p_semAdd) {
        current = current->s_next;
    }
    if (current->s_semAdd != p->p_semAdd) return NULL;
    return outProcQ(&(current->s_procQ), p);
}

/* =======================================================
   Function: headBlocked()
   Purpose: Returns the first process in a semaphore queue without removing it.
   Parameters: Semaphore address
   Returns: Pointer to process, or NULL if queue is empty
   ======================================================= */
pcb_t *headBlocked(int *semAdd) {
    semd_t *current = semd_h->s_next;
    while (current->s_semAdd < semAdd) {
        current = current->s_next;
    }
    if (current->s_semAdd != semAdd) return NULL;
    return headProcQ(current->s_procQ);
}
