#ifndef QUEUE_H
#define QUEUE_H

#include "../h/types.h"

/* ========================================================
    Node Structure for Queues
   ======================================================== */
typedef struct node_t {
    struct node_t *next;
    struct node_t *prev;
} node_t;

/* Queue structure with a single tail pointer */
typedef struct queue_t {
    node_t *tail; 
} queue_t;

/* Function Prototypes */
void initQueue(queue_t *q);
int isEmptyQueue(queue_t *q);
void insertIntoQueue(queue_t *q, node_t *newNode);
node_t *removeFromQueue(queue_t *q);
node_t *removeSpecificFromQueue(queue_t *q, node_t *target);
node_t *getHeadOfQueue(queue_t *q);

#endif
