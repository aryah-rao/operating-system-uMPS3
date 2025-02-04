#ifndef PCB
#define PCB
/************************* PROCQ.H *****************************
*
* The externals declaration file for the Process Control Block
* Module.
*
* Written by Mikeyg
*/
#include "../h/types.h"
extern void freePcb (pcb_PTR *p);                       /* Free a PCB */
extern pcb_PTR *allocPcb ();                            /* Allocate a PCB */
extern void initPcbs ();                                /* Initialize the PCB list */

extern pcb_PTR *mkEmptyProcQ ();                        /* Create an empty process queue */
extern int emptyProcQ (pcb_PTR *tp);                    /* Check if queue is empty */
extern void insertProcQ (pcb_PTR **tp, pcb_PTR *p);     /* Insert PCB into queue */
extern pcb_PTR *removeProcQ (pcb_PTR **tp);             /* Remove first PCB from queue */
extern pcb_PTR *outProcQ (pcb_PTR **tp, pcb_PTR *p);    /* Remove specific PCB */
extern pcb_PTR *headProcQ (pcb_PTR *tp);                /* Get first PCB without removing */

extern int emptyChild (pcb_PTR *p);
extern void insertChild (pcb_PTR *prnt, pcb_PTR *p);    /* Insert child into tree */
extern pcb_PTR *removeChild (pcb_PTR *p);               /* Remove first child */
extern pcb_PTR *outChild (pcb_PTR *p);                  /* Remove specific child */
/***************************************************************/

#endif