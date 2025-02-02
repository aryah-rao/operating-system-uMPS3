#ifndef TREE_H
#define TREE_H

#include "../h/queue.h"

/* Function Prototypes */
void initTree(pcb_PTR root);
void insertChild(pcb_PTR parent, pcb_PTR child);
pcb_PTR removeChild(pcb_PTR parent);
pcb_PTR removeSpecificChild(pcb_PTR child);
int isLeaf(pcb_PTR p);

#endif /* TREE_H */
