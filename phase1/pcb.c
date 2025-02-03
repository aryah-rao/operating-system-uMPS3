/******************************* PCB.c *************************************
*
* Module: Process Control Block (PCB) Management
*
* Written by Aryah Rao & Anish Reddy (Cleaned Version)
*
* This module provides process management functions:
*
*****************************************************************************/

#include "../h/pcb.h"

/******************** Hidden Global Variables **********************/

static pcb_t pcbFreeTable[MAXPROC]; /* Static array of PCBs */
static pcb_t *pcbFreeTail = NULL;     /* Tail pointer for the free PCB list (circular DLL) */


/********************* Hidden Helper Functions *********************/

/* ========================================================================
* Function: enqueue
*
* Description: Inserts the PCB pointed to by p into the process queue.
*
* Parameters:
*               tail - Pointer to process queue
*               p - Pointer to PCB
*
* Returns:
*               None
*  ======================================================================== */
static void enqueue(pcb_t **tail, pcb_t *p) {
    if (*tail == NULL) {
        *tail = p;
        p->p_next = p;
        p->p_prev = p;
    } else {
        p->p_next = (*tail)->p_next;
        p->p_prev = *tail;
        (*tail)->p_next->p_prev = p;
        (*tail)->p_next = p;
        *tail = p; /* Update tail to the new element */
    }
}

/* ========================================================================
 * Function: dequeue
 *
 * Description: Removes the first (head) PCB from the process queue.
 *
 * Parameters:
 *               tail - Pointer to process queue
 * 
 * Returns:
 *               Pointer to the removed PCB
 * ======================================================================== */

static pcb_t *dequeue(pcb_t **tail) {
    pcb_t *head;
    if (*tail == NULL)
        return NULL;
    head = (*tail)->p_next;
    if (head == *tail) {  /* Only one element in the queue */
        *tail = NULL;
    } else {
        (*tail)->p_next = head->p_next;
        head->p_next->p_prev = *tail;
    }
    head->p_next = head->p_prev = NULL;
    return head;
}

/* ========================================================================
 * Function: removeFromQueue
 *
 * Description: Removes the PCB pointed to by p from the process queue.
 *
 * Parameters:
 *               tail - Pointer to process queue
 *               p - Pointer to PCB
 * 
 * Returns:
 *               Pointer to the removed PCB
 * ======================================================================== */
static pcb_t *removeFromQueue(pcb_t **tail, pcb_t *p) {
    if (tail == NULL || *tail == NULL || p == NULL)
        return NULL;
    
    /* Safety check: if p->p_next or p->p_prev are NULL, then p is not in any list */
    if (p->p_next == NULL || p->p_prev == NULL)
        return NULL;
    
    /* p is in the list; perform removal in constant time */
    if (p->p_next == p && p->p_prev == p) {  /* p is the only element in the list */
        *tail = NULL;
    } else {
        /* Unlink p from its neighbors */
        p->p_prev->p_next = p->p_next;
        p->p_next->p_prev = p->p_prev;
        /* Update the tail pointer if necessary */
        if (*tail == p)
            *tail = p->p_prev;
    }
    /* Clear p's links to mark it as not being in any list */
    p->p_next = p->p_prev = NULL;
    return p;
}

/* ========================================================================
 * Function: resetPcb
 *
 * Description: Resets all fields of a PCB to their initial values.
 * 
 * Parameters:
 *               p - Pointer to the PCB to be reset
 * 
 * Returns:
 *               None
 * ======================================================================== */
static void resetPcb(pcb_t *p) {
    if (p == NULL)
        return;
    
    p->p_next           = NULL;
    p->p_prev           = NULL;
    p->p_prnt           = NULL;
    p->p_child          = NULL;
    p->p_sib            = NULL;
    p->p_time           = 0;
    p->p_s.s_entryHI    = 0;
    p->p_s.s_cause      = 0;
    p->p_s.s_status     = 0;
    p->p_s.s_pc         = 0;
    
    int i;

    /* Reset all state registers */
    for (i = 0; i < STATEREGNUM; i++)
        p->p_s.s_reg[i] = 0;
    
    p->p_semAdd         = NULL;
}


/******************** PCB Global Functions ********************/

/* ========================================================================
 * Function: freePcb

 * Description: Frees the PCB pointed to by p.
 *
 * Parameters:
 *               p - Pointer to PCB
 * 
 * Returns:
 *               None
 * ======================================================================== */
void freePcb(pcb_t *p) {
    if (p == NULL)
        return;
    /* Use the queue helper to insert p into the free list */
    insertProcQ(&pcbFreeTail, p);
}

/* ========================================================================
 * Function: allocPcb
 *
 * Description: Allocates a PCB from the free list.
 * 
 * Parameters:
 *               None
 * 
 * Returns:
 *               Pointer to the allocated PCB
 * ======================================================================== */
pcb_t *allocPcb() {
    if (pcbFreeTail == NULL)
        return NULL;
    
    /* Remove the head element from the free list (which is a circular DLL) */
    pcb_t *p = removeProcQ(&pcbFreeTail);
    
    /* Reset all fields of the PCB using resetPcb() */
    resetPcb(p);
    
    return p;
}

/* ========================================================================
 * Function: initPcbs
 *
 * Description: Initializes the free list of PCBs.
 * 
 * Parameters:
 *               None
 * 
 * Returns:
 *               None
 * ======================================================================== */
void initPcbs() {
    pcbFreeTail = NULL;
    int i;
    for (i = 0; i < MAXPROC; i++) {
        /* Reset the PCB */
        resetPcb(&pcbFreeTable[i]);

        /* Insert into the free list using the circular DLL helper */
        insertProcQ(&pcbFreeTail, &pcbFreeTable[i]);
    }
}

/* ========================================================================
* Function: mkEmptyProcQ
*
* Description: Creates an empty process queue.  
*
* Parameters:
*               None
*
* Returns:
*               Pointer to an empty process queue   
 * ======================================================================== */
pcb_t *mkEmptyProcQ() {
    return NULL;
}

/* ========================================================================
 * Function: emptyProcQ
 *
 * Description: Checks if the process queue is empty.
 * 
 * Parameters:
 *               tp - Pointer to process queue
 * 
 * Returns:
 *               1 if the process queue is empty; 0 otherwise
 * ======================================================================== */
int emptyProcQ(pcb_t *tp) {
    return (tp == NULL);
}

/* ========================================================================
 * Function: insertProcQ
 *
 * Description: Inserts the PCB pointed to by p into the process queue.
 * 
 * Parameters:
 *               tp - Pointer to process queue
 *               p - Pointer to PCB
 * 
 * Returns:
 *               None
 * ======================================================================== */
void insertProcQ(pcb_t **tp, pcb_t *p) {
    enqueue(tp, p);
}

/* ========================================================================
 * Function: removeProcQ
 *
 * Description: Removes the first (head) PCB from the process queue.
 * 
 * Parameters:
 *               tp - Pointer to process queue
 * 
 * Returns:
 *               Pointer to the removed PCB
 *               NULL if the process queue is empty
 * ======================================================================== */
pcb_t *removeProcQ(pcb_t **tp) {
    return dequeue(tp);
}

/* ========================================================================
 * Function: outProcQ
 *
 * Description: Removes the PCB pointed to by p from the process queue.
 * 
 * Parameters:
 *               tp - Pointer to process queue
 *               p - Pointer to PCB
 * 
 * Returns:
 *               Pointer to the removed PCB
 *               NULL if the process queue is empty
 *               NULL if p is not in the process queue
 *
 * ======================================================================== */
pcb_t *outProcQ(pcb_t **tp, pcb_t *p) {
    return removeFromQueue(tp, p);
}

/* ========================================================================
 * Function: headProcQ
 *
 * Description: Returns the first (head) PCB from the process queue.
 * 
 * Parameters:
 *               tp - Pointer to process queue
 * 
 * Returns:
 *               Pointer to the head of the process queue
 *               NULL if the process queue is empty
 * ======================================================================== */
pcb_t *headProcQ(pcb_t *tp) {
    if (tp == NULL)
        return NULL;
    return tp->p_next;
}

/* ========================================================================
 * Function: emptyChild
 *
 * Description: Checks if the child list of the PCB pointed to by p is empty.
 * 
 * Parameters:
 *               p - Pointer to PCB
 * 
 * Returns:
 *               1 if the child list is empty; 0 otherwise
 * ======================================================================== */
int emptyChild(pcb_t *p) {
    if (p == NULL)
        return 1;  /* TRUE */
    return (p->p_child == NULL);
}

/* ========================================================================
 * Function: insertChild
 *
 * Description: Inserts the PCB pointed to by p into the child list of the
 *              PCB pointed to by prnt.
 * 
 * Parameters:
 *               prnt - Pointer to parent PCB
 *               p - Pointer to child PCB
 * 
 * Returns:
 *               None
 * ======================================================================== */
void insertChild(pcb_t *prnt, pcb_t *p) {
    if (prnt == NULL || p == NULL)
        return;
    p->p_prnt = prnt;
    /* Insert p into prnt's child list (which is maintained as a circular DLL).
       The parent's child pointer is treated as the tail pointer. */
    enqueue(&(prnt->p_child), p);
}

/* ========================================================================
 * Function: removeChild
 *
 * Description: Removes the first (head) PCB from the child list of the
 *              PCB pointed to by p.
 * 
 * Parameters:
 *               p - Pointer to parent PCB
 * 
 * Returns:
 *               Pointer to the removed child
 *               NULL if the child list is empty
 * ======================================================================== */
pcb_t *removeChild(pcb_t *p) {
    pcb_t *child;
    if (p == NULL || p->p_child == NULL)
        return NULL;
    child = dequeue(&(p->p_child));
    if (child != NULL)
        child->p_prnt = NULL;
    return child;
}

/* ========================================================================
 * Function: outChild
 *
 * Description: Removes the PCB pointed to by p from the child list of the
 *              PCB pointed to by p's parent.
 * 
 * Parameters:
 *               p - Pointer to child PCB
 * 
 * Returns:
 *               Pointer to the removed child
 *               NULL if the child list is empty
 *               NULL if p is not in the child list
   ======================================================================== */
pcb_t *outChild(pcb_t *p) {
    pcb_t **childQueue;
    if (p == NULL || p->p_prnt == NULL)
        return NULL;

    childQueue = &(p->p_prnt->p_child);
    if (removeFromQueue(childQueue, p) != NULL)
        p->p_prnt = NULL;
    return p;
}