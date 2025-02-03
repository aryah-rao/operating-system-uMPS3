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
extern void freePcb (pcb_t *p);          /* Free a PCB */
extern pcb_t *allocPcb ();               /* Allocate a PCB */
extern void initPcbs ();                  /* Initialize the PCB list */

extern pcb_t *mkEmptyProcQ ();           /* Create an empty process queue */
extern int emptyProcQ (pcb_t *tp);       /* Check if queue is empty */
extern void insertProcQ (pcb_t **tp, pcb_t *p); /* Insert PCB into queue */
extern pcb_t *removeProcQ (pcb_t **tp);  /* Remove first PCB from queue */
extern pcb_t *outProcQ (pcb_t **tp, pcb_t *p);  /* Remove specific PCB */
extern pcb_t *headProcQ (pcb_t *tp);     /* Get first PCB without removing */

extern int emptyChild (pcb_t *p);
extern void insertChild (pcb_t *prnt, pcb_t *p);  /* Insert child into tree */
extern pcb_t *removeChild (pcb_t *p);    /* Remove first child */
extern pcb_t *outChild (pcb_t *p);       /* Remove specific child */
/***************************************************************/

#endif