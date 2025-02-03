/******************************* PCB.c *************************************
*
* Module: Process Control Block (PCB) Management
*
* Written by Aryah Rao & Anish Reddy
*
* This module provides process management functions:
* - Allocating and freeing PCBs.
* - Managing process queues (ready/blocked).
* - Handling parent-child relationships.
*
* Uses:
* - Circular doubly linked list (with tail pointer) for process queues.
* - N-ary tree (linked list for children) for parent-child tree operations.
*****************************************************************************/

#include "../h/const.h"
#include "../h/types.h"
#include "../h/pcb.h"

/* ==========================
   Hidden Global Variables
   ========================== */
static pcb_t pcbFreeTable[MAXPROC]; /* Static array of PCBs */
static pcb_t *pcbFree_h;            /* Head of free PCB list */

/* ==========================
   Hidden Helper Functions
   ========================== */

/* =======================================================
   Circular Doubly Linked List (Queue) Functions
   ======================================================= */

/* Insert a PCB into a circular queue with tail pointer */
static void insertQueue(pcb_t **tail, pcb_t *p) {
    if (*tail == NULL) { /* Empty queue case */
        *tail = p;
        p->p_next = p->p_prev = p;
    } else {
        p->p_next = (*tail)->p_next;
        p->p_prev = *tail;
        (*tail)->p_next->p_prev = p;
        (*tail)->p_next = p;
        *tail = p;
    }
}

/* Remove and return the head of the circular queue */
static pcb_t *removeHeadQueue(pcb_t **tail) {
    if (*tail == NULL) return NULL;
    pcb_t *head = (*tail)->p_next;

    if (*tail == head) { /* Only one element */
        *tail = NULL;
    } else {
        (*tail)->p_next = head->p_next;
        head->p_next->p_prev = *tail;
    }

    head->p_next = head->p_prev = NULL;
    return head;
}

/* Remove and return a specific PCB from a queue */
static pcb_t *removeSpecificQueue(pcb_t **tail, pcb_t *p) {
    if (*tail == NULL) return NULL;

    if (*tail == p && p->p_next == p) { /* Single element case */
        *tail = NULL;
    } else {
        p->p_prev->p_next = p->p_next;
        p->p_next->p_prev = p->p_prev;
        if (*tail == p) *tail = p->p_prev;
    }

    p->p_next = p->p_prev = NULL;
    return p;
}

/* =======================================================
   N-ary Tree Functions (Process Tree)
   ======================================================= */

/* Insert a child PCB into the parent's tree */
static void insertTree(pcb_t *parent, pcb_t *child) {
    child->p_prnt = parent;
    child->p_sib = NULL;

    if (parent->p_child == NULL) {
        parent->p_child = child;
    } else {
        pcb_t *sibling = parent->p_child;
        while (sibling->p_sib != NULL) {
            sibling = sibling->p_sib;
        }
        sibling->p_sib = child;
    }
}

/* Remove and return the first child of a PCB */
static pcb_t *removeFirstChild(pcb_t *parent) {
    if (parent->p_child == NULL) return NULL;
    pcb_t *firstChild = parent->p_child;
    parent->p_child = firstChild->p_sib;
    firstChild->p_prnt = NULL;
    firstChild->p_sib = NULL;
    return firstChild;
}

/* Remove and return a specific child from its parent */
static pcb_t *removeSpecificChild(pcb_t *p) {
    if (p->p_prnt == NULL) return NULL;
    pcb_t *parent = p->p_prnt;

    if (parent->p_child == p) {
        parent->p_child = p->p_sib;
    } else {
        pcb_t *sibling = parent->p_child;
        while (sibling->p_sib != NULL && sibling->p_sib != p) {
            sibling = sibling->p_sib;
        }
        if (sibling->p_sib == p) {
            sibling->p_sib = p->p_sib;
        } else {
            return NULL; /* Not found */
        }
    }

    p->p_prnt = NULL;
    p->p_sib = NULL;
    return p;
}

   /* ==========================
   Global Functions
   ========================== */

/* =======================================================
   Function: initPcbs()

   Purpose: Initializes the PCB free list.

   Parameters: None

   Returns: None
   ======================================================= */
void initPcbs() {
    int i;
    for (i = 0; i < MAXPROC - 1; i++) {
        pcbFreeTable[i].p_next = &pcbFreeTable[i + 1];
    }
    pcbFreeTable[MAXPROC - 1].p_next = NULL;
    pcbFree_h = &pcbFreeTable[0];
}

/* =======================================================
   Function: allocPcb()

   Purpose: Allocates a PCB from the free list.

   Parameters: None

   Returns: Pointer to allocated PCB, or NULL if empty.
   ======================================================= */
pcb_t *allocPcb() {
    if (!pcbFree_h) return NULL;
    pcb_t *p = pcbFree_h;
    pcbFree_h = pcbFree_h->p_next;

    /* Reset all fields */
    p->p_next = p->p_prev = NULL;
    p->p_prnt = p->p_child = p->p_sib = NULL;
    p->p_semAdd = NULL;
    p->p_time = 0;
    /*p->p_supportStruct = NULL;*/ /* Not used */

    return p;
}

/* =======================================================
   Function: freePcb()

   Purpose: Frees a PCB and returns it to the free list.

   Parameters: pcb_t *p - PCB to be freed.

   Returns: None.
   ======================================================= */
void freePcb(pcb_t *p) {
    p->p_next = pcbFree_h;
    pcbFree_h = p;
}

/* =======================================================
   Function: mkEmptyProcQ()

   Purpose: Initializes a process queue.

   Parameters: None.

   Returns: NULL (since tail pointer represents an empty queue).
   ======================================================= */
pcb_t *mkEmptyProcQ() {
    return NULL;
}

/* =======================================================
   Function: emptyProcQ()

   Purpose: Checks if a process queue is empty.

   Parameters: pcb_t *tp - Tail pointer to queue.

   Returns: TRUE if queue is empty, FALSE otherwise.
   ======================================================= */
int emptyProcQ(pcb_t *tp) {
    return tp == NULL;
}

/* =======================================================
   Function: insertProcQ()

   Purpose: Inserts a PCB into a queue.

   Parameters: pcb_t **tp - Tail pointer to queue.
               pcb_t *p - PCB to insert.

   Returns: None.
   ======================================================= */
void insertProcQ(pcb_t **tp, pcb_t *p) {
    insertQueue(tp, p);
}

/* =======================================================
   Function: removeProcQ()

   Purpose: Removes the first PCB from a queue.

   Parameters: pcb_t **tp - Tail pointer to queue.

   Returns: Pointer to removed PCB, or NULL if empty.
   ======================================================= */
pcb_t *removeProcQ(pcb_t **tp) {
    return removeHeadQueue(tp);
}

/* =======================================================
   Function: insertChild()

   Purpose: Inserts a child into a process tree.

   Parameters: pcb_t *parent - Parent process.
               pcb_t *child - Child process.

   Returns: None.
   ======================================================= */
void insertChild(pcb_t *parent, pcb_t *child) {
    insertTree(parent, child);
}

/* =======================================================
   Function: removeChild()

   Purpose: Removes the first child of a process.

   Parameters: pcb_t *p - Parent process.

   Returns: Pointer to removed child PCB, or NULL if no children.
   ======================================================= */
pcb_t *removeChild(pcb_t *p) {
    return removeFirstChild(p);
}

/* =======================================================
   Function: outChild()

   Purpose: Removes a specific child from its parent.

   Parameters: pcb_t *p - Child process.

   Returns: Pointer to removed PCB, or NULL if not found.
   ======================================================= */
pcb_t *outChild(pcb_t *p) {
    return removeSpecificChild(p);
}
