/******************************* TREE.c *************************************
*
* Module: N-Ary Tree for Process Control Blocks (PCB) using Queue Module
*
* Written by Aryah Rao & Anish Reddy
*
* This module provides functions for managing parent-child relationships
* of PCBs in an N-ary tree structure, using a queue for siblings.
*
* Features:
* - Each process (PCB) can have multiple children.
* - Each parent stores a queue of children.
* - Queue operations are handled by queue.c.
*****************************************************************************/

#include "../h/tree.h"
#include "../h/queue.h"
#include "../h/pcb.h"

/* =======================================================
   Function: initTree()

   Purpose: Initializes a PCB as a root node (no parent, no children).

   Parameters:
        - root: Pointer to the root PCB.

   Returns: None
   ======================================================= */
void initTree(pcb_PTR root) {
    root->p_prnt = NULL;
    initQueue(&(root->children)); // Initialize the queue for child processes
}

/* =======================================================
   Function: insertChild()

   Purpose: Inserts a child into a parent's child queue.

   Parameters:
        - parent: Pointer to the parent PCB.
        - child: Pointer to the child PCB.

   Returns: None
   ======================================================= */
void insertChild(pcb_PTR parent, pcb_PTR child) {
    child->p_prnt = parent;
    insertIntoQueue(&(parent->children), (node_t *)child);
}

/* =======================================================
   Function: removeChild()

   Purpose: Removes and returns the first child from a parent’s queue.

   Parameters:
        - parent: Pointer to the parent PCB.

   Returns: Pointer to the removed child PCB, or NULL if no children.
   ======================================================= */
pcb_PTR removeChild(pcb_PTR parent) {
    return (pcb_PTR)removeFromQueue(&(parent->children));
}

/* =======================================================
   Function: removeSpecificChild()

   Purpose: Removes a specific child PCB from the parent's child queue.

   Parameters:
        - child: Pointer to the child PCB to remove.

   Returns: Pointer to removed PCB, or NULL if not found.
   ======================================================= */
pcb_PTR removeSpecificChild(pcb_PTR child) {
    return (pcb_PTR)removeSpecificFromQueue(&(child->p_prnt->children), (node_t *)child);
}

/* =======================================================
   Function: isLeaf()

   Purpose: Checks if a PCB has no children.

   Parameters:
        - p: Pointer to PCB.

   Returns: 1 (TRUE) if PCB has no children, 0 (FALSE) otherwise.
   ======================================================= */
int isLeaf(pcb_PTR p) {
    return isEmptyQueue(&(p->children));
}
