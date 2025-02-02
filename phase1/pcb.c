
#include "../h/pcb.h"


static pcb_t pcbTable[MAXPROC];

static pcb_PTR pcbFree_h;

void freePcb (pcb_PTR p){
	p->next = pcbFree_h;
	pcbFree_h = p;
}
pcb_PTR allocPcb (){
	if (pcbFree_h == NULL){
		return NULL;
	}
	pcb_PTR p = pcbFree_h;
	pcbFree_h = pcbFree_h->next;

	p->next = NULL;
	p->prev = NULL;
	p->parent = NULL;
	p->sibling = NULL;
	p->child = NULL;
	p->p_semAdd = NULL;
	p->p_time = NULL;

}
void initPcbs (){
	pcbFree_h = NULL;
	int i;
	for (i = 0; i < MAXPROC; i++){
		freePcb(&pcbTable[i]);
	}
}

pcb_PTR mkEmptyProcQ (){
	return NULL;
}
int emptyProcQ (pcb_PTR tp){
	return (tp == NULL);
}
void insertProcQ (pcb_PTR *tp, pcb_PTR p){
	if (*tp == NULL){
		p->next = p;
		p->prev = p;
		*tp = p;
	}
	else{
		p->next = (*tp)->next;
		p->prev = *tp;
		(*tp)->next->prev = p;
		(*tp)->next = p;
		*tp = p;
	}
}
pcb_PTR removeProcQ (pcb_PTR *tp){
	if (*tp == NULL){
		return NULL;
	}
	return outProcQ(tp, (*tp)->next);

}
pcb_PTR outProcQ (pcb_PTR *tp, pcb_PTR p){
	if (*tp == NULL || p == NULL) {
        return NULL;
    }

    if (p->next == p) {
        *tp = NULL;
    } else {
        p->prev->next = p->next;
        p->next->prev = p->prev;
       
        if (*tp == p) {
            *tp = p->prev;
        }
    }

    p->next = NULL;
    p->prev = NULL;
   
    return p;

}
pcb_PTR headProcQ (pcb_PTR tp){
	if (tp == NULL){
		return NULL;
	}
	return tp->next;
}

int emptyChild (pcb_PTR p){
	return (p->child == NULL);
}
void insertChild (pcb_PTR prnt, pcb_PTR p){
	p->parent = prnt;
	p->sibling = prnt->child;
	prnt->child = p;
}
pcb_PTR removeChild (pcb_PTR p){
	if (p->child == NULL){
		return NULL;
	}
	pcb_PTR child = p->child;
	p->child = child->sibling;
	child->parent = NULL;
	child->sibling = NULL;
	return child;

}
pcb_PTR outChild (pcb_PTR p){
 return NULL;
}

