/******************************* QUEUE.c *************************************
*
* Module: Doubly Linked Circular Queue with 2 Sentinel Nodes
*
* Written by Aryah Rao & Anish Reddy
*
* This module provides a queue implementation for PCB and ASL.
* - The queue is circular and doubly linked.
* - It has only a tail pointer (no head pointer).
* - Two sentinel nodes are used to simplify insertion/removal logic.
*
*****************************************************************************/

#include "../h/queue.h"
#include "../h/pcb.h"
#include "../h/types.h"
#include "../h/const.h"

/* =======================================================
   Function: initQueue()

   Purpose: Initializes a queue with two sentinel nodes.

   Parameters:
        - q: Pointer to queue structure.

   Returns: None
   ======================================================= */
void initQueue(queue_t *q) {
    q->tail = (node_t *)allocPcb(); // Allocate memory for tail sentinel
    node_t *sentinel1 = (node_t *)allocPcb();
    node_t *sentinel2 = (node_t *)allocPcb();

    q->tail->next = sentinel1;
    q->tail->prev = sentinel2;
    sentinel1->next = sentinel2;
    sentinel1->prev = q->tail;
    sentinel2->next = q->tail;
    sentinel2->prev = sentinel1;
}

/* =======================================================
   Function: isEmptyQueue()

   Purpose: Checks if the queue is empty.

   Parameters:
        - q: Pointer to queue structure.

   Returns: 1 (TRUE) if empty, 0 (FALSE) otherwise.
   ======================================================= */
int isEmptyQueue(queue_t *q) {
    return (q->tail->next->next == q->tail);
}

/* =======================================================
   Function: insertIntoQueue()

   Purpose: Inserts a node into the queue at the tail.

   Parameters:
        - q: Pointer to queue.
        - newNode: Pointer to the new node to insert.

   Returns: None
   ======================================================= */
void insertIntoQueue(queue_t *q, node_t *newNode) {
    node_t *first = q->tail->next;
    
    newNode->next = first;
    newNode->prev = q->tail;
    q->tail->next = newNode;
    first->prev = newNode;
}

/* =======================================================
   Function: removeFromQueue()

   Purpose: Removes and returns the first node from the queue.

   Parameters:
        - q: Pointer to queue.

   Returns: Pointer to the removed node, or NULL if empty.
   ======================================================= */
node_t *removeFromQueue(queue_t *q) {
    if (isEmptyQueue(q)) return NULL;

    node_t *first = q->tail->next;
    q->tail->next = first->next;
    first->next->prev = q->tail;

    first->next = NULL;
    first->prev = NULL;
    return first;
}

/* =======================================================
   Function: removeSpecificFromQueue()

   Purpose: Removes a specific node from the queue.

   Parameters:
        - q: Pointer to queue.
        - target: Node to remove.

   Returns: Pointer to removed node, or NULL if not found.
   ======================================================= */
node_t *removeSpecificFromQueue(queue_t *q, node_t *target) {
    node_t *current = q->tail->next;

    while (current != q->tail) {
        if (current == target) {
            current->prev->next = current->next;
            current->next->prev = current->prev;
            current->next = NULL;
            current->prev = NULL;
            return current;
        }
        current = current->next;
    }
    return NULL; // Target not found
}

/* =======================================================
   Function: getHeadOfQueue()

   Purpose: Returns the first node without removing it.

   Parameters:
        - q: Pointer to queue.

   Returns: Pointer to the first node, or NULL if empty.
   ======================================================= */
node_t *getHeadOfQueue(queue_t *q) {
    if (isEmptyQueue(q)) return NULL;
    return q->tail->next;
}
