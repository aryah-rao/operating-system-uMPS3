/******************************* PCB.c *************************************
*
* Module: Process Control Block (PCB)
*
* Written by Aryah Rao & Anish Reddy
*
* We provide functions for the Process Control Block (PCB):
*
* allocPcb - Allocate a PCB
* freePcb - Free a PCB
* initPcbs - Initialize the free list of PCBs
* mkEmptyProcQ - Create an empty process queue
* emptyProcQ - Check if the process queue is empty
* insertProcQ - Insert a PCB into the process queue
* removeProcQ - Remove the first PCB from the process queue
* outProcQ - Remove a specific PCB from the process queue
* headProcQ - Get the head of the process queue
* emptyChild - Check if the child list is empty
* insertChild - Insert a child into the child list of a parent
* removeChild - Remove the first child from the child list
* outChild - Remove a specific child from the child list
* 
* The free list of PCBs is implemented as a circular DLL.
* The child list of a parent is implemented as a circular DLL.
*
*****************************************************************************/


#include "../h/pcb.h"


/******************** Hidden Global Variables **********************/


static pcb_PTR pcbFreeTable[MAXPROC];   /* Static array of PCBs */
static pcb_PTR *pcbFreeTail = NULL;     /* Tail pointer for the free PCB list (circular DLL) */


/********************* Hidden Helper Functions *********************/


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
static void resetPcb(pcb_PTR *p) {
    /* Check if p is NULL */
    if (p == NULL)
        return;
    
    /* Reset all fields to their initial values */
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

    /* Reset all state registers */
    int i;
    for (i = 0; i < STATEREGNUM; i++)
        p->p_s.s_reg[i] = 0;
    
    p->p_semAdd         = NULL;
    /*p->p_supportStruct  = 0;*/
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
void freePcb(pcb_PTR *p) {
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
pcb_PTR *allocPcb() {
    if (pcbFreeTail == NULL)
        return NULL;
    
    /* Remove the head element from the free list (which is a circular DLL) */
    pcb_PTR *p = removeProcQ(&pcbFreeTail);
    
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
pcb_PTR *mkEmptyProcQ() {
    return NULL; /* Empty process queue */
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
int emptyProcQ(pcb_PTR *tp) {
    /* Return if the process queue is empty */
    return (tp == NULL);
}


/* ========================================================================
 * Function: insertProcQ
 *
 * Description: Inserts the PCB pointed to by p into the process queue.
 * 
 * Parameters:
 *               tp - Pointer to process queue tail
 *               p - Pointer to PCB
 * 
 * Returns:
 *               None
 * ======================================================================== */
void insertProcQ(pcb_PTR **tp, pcb_PTR *p) {
    /* Check if the process queue is empty */
    if (*tp == NULL) {
        /* The process queue is empty; insert p as the only element */
        *tp = p;
        p->p_next = p;  /* Circular link */
        p->p_prev = p;
    } else {
        /* Insert p at the end of the queue */
        p->p_next = (*tp)->p_next;
        p->p_prev = *tp;
        (*tp)->p_next->p_prev = p;
        (*tp)->p_next = p;
        *tp = p;  /* Update tail */
    }
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
pcb_PTR *removeProcQ(pcb_PTR **tp) {
    /* Check if the process queue is empty */
    if (*tp == NULL) {
        return NULL;
    }

    /* Remove the first process using outProcQ() */
    return outProcQ(tp, (*tp)->p_next);
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
pcb_PTR *outProcQ(pcb_PTR **tp, pcb_PTR *p) {
    /* Check if the process queue or the process is NULL */
    if (*tp == NULL || p == NULL) {
        return NULL;
    }

    /* Ensure p is actually in the queue (preventing segmentation faults) */
    if (p->p_next == NULL || p->p_prev == NULL) {
        return NULL;
    }

    /* Remove p from the queue */
    if (p->p_next == p && p->p_prev == p) {  /* Only one element */
        *tp = NULL;  /* Queue is now empty */
    } else {
        /* Unlink p from the list */
        p->p_prev->p_next = p->p_next;
        p->p_next->p_prev = p->p_prev;

        /* Update tail if necessary */
        if (*tp == p) {
            *tp = p->p_prev;
        }
    }

    /* Clear p's links */
    p->p_next = p->p_prev = NULL;
    return p;
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
pcb_PTR *headProcQ(pcb_PTR *tp) {
    /* Check if the process queue is empty */
    if (tp == NULL)
        return NULL;

    /* Return the head of the process queue */
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
int emptyChild(pcb_PTR *p) {
    /* Return if the child list is empty */
    if (p == NULL)
        return 1;  /* TRUE */
    
    /* Return if the child list is not empty */
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
void insertChild(pcb_PTR *prnt, pcb_PTR *p) {
    /* Check if the parent or child is NULL */
    if (prnt == NULL || p == NULL)
        return;

    /* Set the parent of p */
    p->p_prnt = prnt;

    /* Insert p into prnt's child list using insertProcQ() */
    insertProcQ(&(prnt->p_child), p);
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
pcb_PTR *removeChild(pcb_PTR *p) {
    /* Check if the parent PCB or its child list is empty */
    if (p == NULL || p->p_child == NULL) {
        return NULL;
    }

    /* Use outChild() to remove the first child */
    return outChild(p->p_child);
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
pcb_PTR *outChild(pcb_PTR *p) {
    /* Check if the parent or child is NULL */
    if (p == NULL || p->p_prnt == NULL)
        return NULL;

    /* Remove p from its parent's child list */
    pcb_PTR **childQueue = &(p->p_prnt->p_child);

    /* Use outProcQ() to remove p from the child list */
    if (outProcQ(childQueue, p) != NULL) {
        p->p_prnt = NULL;  /* Clear parent reference */
    }

    /* Return the removed child */
    return p;
}
