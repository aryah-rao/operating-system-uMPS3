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
int deviceSemaphores[DEVICE_COUNT+1];  /* Array of device semaphores + pseudo-clock */
cpu_t startTOD;                      /* Time of day at system start */

/* External declaration for the test function provided by Phase 2 */
extern void test();
extern void uTLB_RefillHandler();

void main() {

    /* Initialize Pass Up Vector */
    passupvector_t *passupvector = (passupvector_t *)PASSUPVECTOR;
    /* Set TLBL-Refill Handler */
    passupvector->tlb_refll_handler = (memaddr)uTLB_RefillHandler;
    /* Set TLBL-Refill Stack Pointer */
    passupvector->tlb_refll_stackPtr = KERNEL_STACK;
    /* Set Exception Handler */
    passupvector->execption_handler = (memaddr)exceptionHandler;
    /* Set Exception Stack Pointer */
    passupvector->exception_stackPtr = KERNEL_STACK;

    /* Initialize PCBs */
    initPcbs();
    
    /* Initialize ASL */
    initASL();
    
    /* Initialize global variables */
    processCount = 0;
    softBlockCount = 0;
    /* Initialize ready queue */
    readyQueue = mkEmptyProcQ();
    /* Initialize current process */
    currentProcess = NULL;
    
    /* Initialize device semaphores */
    int i = 0;
    for (i = 0; i < DEVICE_COUNT; i++) {
        deviceSemaphores[i] = 0;
    }

    /* Create and initialize first process */
    pcb_PTR firstProcess = allocPcb();
    if (firstProcess == NULL) {
        PANIC();  /* If no PCBs are available, panic */
    }

    /* Initialize process state */
    /* allocPcb() already initialized all process fields  to 0 */

    /* Set up first process */
    /* Set Program Counter to test */
    firstProcess->p_s.s_pc = (memaddr)test;    
    /* Set t9 to test */
    firstProcess->p_s.s_t9 = (memaddr)test;  
    /* Set status register */
    firstProcess->p_s.s_status = ALLOFF | IECON | IMON | TEBITON;  
    /* Set stack pointer to top of RAM */
    firstProcess->p_s.s_sp = RAMTOP;  
    
    /* Add to ready queue and update process count */
    insertProcQ(&readyQueue, firstProcess);
    processCount++;

    /* Load System-wide interval timer with 100ms interval */
    LDIT(CLOCKINTERVAL);

    /* Read Start time */
    STCK(startTOD);

    /* Call scheduler */
    scheduler();
}

