
#include "../h/pcb.h"


static pcb_t pcbTable[MAXPROC];

static pcb_PTR pcbFree_h;

void freePcb (pcb_PTR p){
	 
}
pcb_PTR allocPcb ();
void initPcbs (){
	pcbFree_h = NULL;
	for (int i = 0; i < MAXPROC; i++){
		freePcb(&pcbTable[i]);
	}
}

pcb_PTR mkEmptyProcQ (){

}
int emptyProcQ (pcb_PTR tp){

}
void insertProcQ (pcb_PTR *tp, pcb_PTR p){

}
pcb_PTR removeProcQ (pcb_PTR *tp){

}
pcb_PTR outProcQ (pcb_PTR *tp, pcb_PTR p){

}
pcb_PTR headProcQ (pcb_PTR tp){

}

int emptyChild (pcb_PTR p){

}
void insertChild (pcb_PTR prnt, pcb_PTR p){

}
pcb_PTR removeChild (pcb_PTR p){

}
pcb_PTR outChild (pcb_PTR p){
	
}

