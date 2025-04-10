#ifndef TYPES
#define TYPES

/**************************************************************************** 
 *
 * This header file contains utility types definitions.
 * 
 * Written by Mikeyg
 * 
 * Modified by Aryah Rao and Anish Reddy
 * 
 ****************************************************************************/

#include "../h/const.h"

typedef signed int 			cpu_t;

typedef unsigned int 		memaddr;

/* Device Register */
typedef struct {
	unsigned int 			d_status;
	unsigned int 			d_command;
	unsigned int 			d_data0;
	unsigned int 			d_data1;
} device_t;

#define t_recv_status		d_status
#define t_recv_command		d_command
#define t_transm_status		d_data0
#define t_transm_command	d_data1


/* Bus Register Area */
typedef struct {
	unsigned int 			rambase;
	unsigned int 			ramsize;
	unsigned int 			execbase;
	unsigned int 			execsize;
	unsigned int 			bootbase;
	unsigned int 			bootsize;
	unsigned int 			todhi;
	unsigned int 			todlo;
	unsigned int 			intervaltimer;
	unsigned int 			timescale;
	unsigned int 			TLB_Floor_Addr;
	unsigned int 			inst_dev[DEVINTNUM];
	unsigned int 			interrupt_dev[DEVINTNUM];
	device_t				devreg[DEVINTNUM * DEVPERINT];
} devregarea_t;


/* Pass Up Vector */
typedef struct passupvector {
    unsigned int 			tlb_refll_handler;
    unsigned int 			tlb_refll_stackPtr;
    unsigned int 			execption_handler;
    unsigned int 			exception_stackPtr;
} passupvector_t;


/* State Structure */
#define STATEREGNUM			31
typedef struct state_t {
	unsigned int			s_entryHI;
	unsigned int			s_cause;
	unsigned int			s_status;
	unsigned int 			s_pc;
	int	 					s_reg[STATEREGNUM];

} state_t, *state_PTR;


/* Context Structure */
typedef struct context_t {
	unsigned int 			c_stackPtr, 	/* stack pointer value */
							c_status, 		/* status reg value */
							c_pc; 			/* PC address */
} context_t, *context_PTR;


/* Page Table Entry */
typedef struct pageTableEntry {
    unsigned int 			pte_entryHI;
    unsigned int 			pte_entryLO;
} pageTableEntry_t, *pageTableEntry_PTR;


/* Support Structure */
typedef struct support_t { 
	int 					sup_asid; 				/* Process Id (asid) */ 
	state_t 				sup_exceptState[2]; 	/* stored excpt states */ 
	context_t 				sup_exceptContext[2]; 	/* pass up contexts */
	pageTableEntry_t 		sup_pageTable[MAXPAGES];/* Page table array (32 PTEs) */
	int 				sup_stackGen[500];         	/* General stack for exceptions */
	int 				sup_stackTLB[500];     	/* Stack for page fault exceptions */
} support_t, *support_PTR;


/* Swap Pool Data Structure */
typedef struct swapPoolEntry_t {
    int 					asid;                  	/* Process ID (ASID) */
    int 					vpn;                   	/* Virtual Page Number */
    int 					valid;                 	/* Entry validity flag */
    pageTableEntry_PTR 		pte;    				/* Pointer to page table entry */
} swapPoolEntry_t, *swapPoolEntry_PTR;


/* Process Control Block */
typedef struct pcb_t{
	/* process queue fields */
	struct pcb_t 			*p_next;
	struct pcb_t 			*p_prev;

	/* process tree fields */
	struct pcb_t 			*p_prnt;
	struct pcb_t 			*p_child;
	struct pcb_t 			*p_next_sib;
	struct pcb_t 			*p_prev_sib;

	/* process state information */
	state_t 				p_s;
	cpu_t 					p_time;
	int 					*p_semAdd;

	/* support layer information */
	support_t 				*p_supportStruct;

	/* MLFQ fields */
	int 					priority;
	int 					earlyExits;
} pcb_t, *pcb_PTR;


/* Semaphore Descriptor */
typedef struct semd_t {
    struct semd_t 			*s_next; 		/* Pointer to next semaphore descriptor */
	struct semd_t 			*s_prev; 		/* Pointer to previous semaphore descriptor */
    int 					*s_semAdd; 		/* Pointer to the Semaphore address */
	pcb_t 					*s_procQ; 		/* Tail Pointer to a Process queue */
} semd_t, *semd_PTR;


/* State Register Aliases */
#define	s_at				s_reg[0]
#define	s_v0				s_reg[1]
#define s_v1				s_reg[2]
#define s_a0				s_reg[3]
#define s_a1				s_reg[4]
#define s_a2				s_reg[5]
#define s_a3				s_reg[6]
#define s_t0				s_reg[7]
#define s_t1				s_reg[8]
#define s_t2				s_reg[9]
#define s_t3				s_reg[10]
#define s_t4				s_reg[11]
#define s_t5				s_reg[12]
#define s_t6				s_reg[13]
#define s_t7				s_reg[14]
#define s_s0				s_reg[15]
#define s_s1				s_reg[16]
#define s_s2				s_reg[17]
#define s_s3				s_reg[18]
#define s_s4				s_reg[19]
#define s_s5				s_reg[20]
#define s_s6				s_reg[21]
#define s_s7				s_reg[22]
#define s_t8				s_reg[23]
#define s_t9				s_reg[24]
#define s_gp				s_reg[25]
#define s_sp				s_reg[26]
#define s_fp				s_reg[27]
#define s_ra				s_reg[28]
#define s_HI				s_reg[29]
#define s_LO				s_reg[30]

/***************************************************************/

#endif
