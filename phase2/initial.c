/**************************** initial.c *****************************
*
* This is the initialization module for Phase 2 of the PandOS project.
* It sets up the system's data structures, exception vectors, and the
* first process.
*
******************************************************************/

/* Storing Status before pre-emptive switch */

#include "../h/const.h"
#include "../h/types.h"
#include "/usr/include/umps3/umps/libumps.h"
#include "../h/pcb.h"
#include "../h/asl.h"
#include "../h/initial.h"
#include "../h/scheduler.h"
#include "../h/exceptions.h"
#include "../h/interrupts.h"

/* Global Variables */
int processCount;                    /* Number of processes in system */
int softBlockCount;                  /* Number of blocked processes */
pcb_PTR readyQueue;                  /* Ready queue */
pcb_PTR currentProcess;              /* Currently executing process */
int deviceSemaphores[DEVICE_COUNT+1];  /* Array of device semaphores */
cpu_t startTOD;                      /* Time of day at system start */

/* External declaration for the test function provided by Phase 2 */
extern void test();

void main() {
    /* Step 1: Initialize PCBs */
    initPcbs();
    
    /* Step 2: Initialize ASL */
    initASL();
    
    /* Step 3: Initialize global variables */
    processCount = 0;
    softBlockCount = 0;
    readyQueue = mkEmptyProcQ();
    currentProcess = NULL;
    
    /* Step 4: Initialize device semaphores */
    int i = 0;
    for (i = 0; i < DEVICE_COUNT; i++) {
        deviceSemaphores[i] = 0;
    }

    /* Step 5: Load system-wide interval timer */
    LDIT(QUANTUM);  /* Set up 5ms quantum */

    /* Step 6: Set up and load exception state areas */
    
    /* Initialize Exception State Areas */
    state_t *excState;
    
    /* A) Set up TLB-Refill Event Handler */
    excState = (state_t *)TLB_NEWAREA;
    excState->s_status = ALLOFF | TE;  /* Status bits all off, TE bit on */
    excState->s_pc = (memaddr)uTLB_RefillHandler;  /* Set PC to handler */
    excState->s_sp = KERNEL_STACK;  /* Set stack pointer */
    
    /* B) Set up Program Trap Handler */
    excState = (state_t *)PGMTRAP_NEWAREA;
    excState->s_status = ALLOFF | TE;
    excState->s_pc = (memaddr)exceptionHandler;
    excState->s_sp = KERNEL_STACK;
    
    /* C) Set up SYSCALL/BreakPoint Handler */
    excState = (state_t *)SYSBK_NEWAREA;
    excState->s_status = ALLOFF | TE;
    excState->s_pc = (memaddr)exceptionHandler;
    excState->s_sp = KERNEL_STACK;
    
    /* D) Set up Interrupt Handler */
    excState = (state_t *)INT_NEWAREA;
    excState->s_status = ALLOFF | TE;
    excState->s_pc = (memaddr)interruptHandler;
    excState->s_sp = KERNEL_STACK;

    /* Step 7: Set up Pass Up Vector */
    passupvector_t *passupvector = (passupvector_t *)PASSUPVECTOR;
    passupvector->tlb_refll_handler = (memaddr)uTLB_RefillHandler;
    passupvector->tlb_refll_stackPtr = KERNEL_STACK;
    passupvector->execption_handler = (memaddr)exceptionHandler;
    passupvector->exception_stackPtr = KERNEL_STACK;

    /* Step 8: Create and initialize first process */
    pcb_PTR p = allocPcb();
    if (p == NULL) {
        PANIC();  /* If no PCBs are available, panic */
    }

    /* Initialize process state */
    /* test is the external function in p2test.c */
    p->p_s.s_pc = p->p_s.s_t9 = (memaddr)test;  /* Set PC and t9 to test */
    p->p_s.s_status = ALLOFF | IECON | IMON | TE;  /* Set status register */
    p->p_s.s_sp = KERNEL_STACK;  /* Set stack pointer */
    
    /* Initialize process fields */
    p->p_supportStruct = NULL;  /* No support structure */
    p->p_time = 0;  /* Reset CPU time */
    p->p_semAdd = NULL;  /* Not blocked on any semaphore */
    
    /* Add to ready queue and update process count */
    insertProcQ(&readyQueue, p);
    processCount++;

    /* Step 9: Record start time */
    STCK(startTOD);

    /* Step 10: Set PLT to 0 */
    setTIMER(0);

    /* Step 11: Enable all interrupts */
    setSTATUS(getSTATUS() | IECON | IMON);

    /* Step 12: Call scheduler */
    scheduler();
}

