/******************************* initial.c *************************************
 *
 * Module: Initialization
 *
 * Description:
 * This module initializes the system's data structures, sets up exception 
 * vectors, and creates the first process. It performs all necessary startup
 * operations for the Nucleus layer to begin execution.
 *
 * Implementation:
 * The module sets up the Pass Up Vector for exceptions, initializes the PCB
 * and ASL data structures, creates the first process running the test function,
 * initializes system semaphores, and finally calls the scheduler to begin
 * execution.
 *
 * Written by Aryah Rao and Anish Reddy
 *
 ***************************************************************************/

/******************** Included Header Files ********************/
#include "../h/initial.h"

/******************** Global Variables ********************/
/* System process management variables */
int processCount;                       /* Number of processes in system */
int softBlockCount;                     /* Number of blocked processes */
pcb_PTR readyQueueHigh;                 /* Ready queue (High Priority) */
pcb_PTR readyQueueLow;                  /* Ready queue (Low Priority) */
pcb_PTR currentProcess;                 /* Currently executing process */
int deviceSemaphores[DEVICE_COUNT];     /* Array of device semaphores */
cpu_t startTOD;                         /* Time of day at system start */

/******************** External Declarations ********************/
/* External declaration for the test & uTLB_RefillHandler function provided by Phase 2 Test */
extern void test();
extern void uTLB_RefillHandler();

/******************** Function Prototypes ********************/
HIDDEN void initializePassUpVector();
HIDDEN void initializeSystemVariables();
HIDDEN pcb_PTR createFirstProcess();

/******************** Function Definitions ********************/

/* ========================================================================
 * Function: main
 *
 * Description: Entry point to the PandOS nucleus. Initializes system data 
 *              structures, creates the first process, and starts the scheduler.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              Does not return
 * ======================================================================== */
void main() {
    /* Initialize Pass Up Vector */
    /* Set up the system's exception handlers and associated stack pointers */
    initializePassUpVector();
    
    /* Initialize PCBs */
    initPcbs();
    
    /* Initialize ASL */
    initASL();
    
    /* Initialize global variables */
    initializeSystemVariables();

    /* Create and initialize first process */
    pcb_PTR firstProcess = createFirstProcess();
    if (firstProcess == mkEmptyProcQ()) {
        PANIC();  /* If no PCBs are available, panic */
    }

    /* Load System-wide interval timer with 100ms interval */
    LDIT(CLOCKINTERVAL);

    /* Read Start time */
    STCK(startTOD);

    /* Call scheduler */
    scheduler();
    
    /* Should never reach here */
}

/* ========================================================================
 * Function: initializePassUpVector
 *
 * Description: Sets up the Pass Up Vector with the address of exception 
 *              handlers and their stack pointers.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              None
 * ======================================================================== */
HIDDEN void initializePassUpVector() {
    passupvector_t *passupvector = (passupvector_t *)PASSUPVECTOR;
    
    /* Set TLBL-Refill Handler */
    passupvector->tlb_refll_handler = (memaddr)uTLB_RefillHandler;
    
    /* Set TLBL-Refill Stack Pointer */
    passupvector->tlb_refll_stackPtr = KERNEL_STACK;
    
    /* Set Exception Handler */
    passupvector->execption_handler = (memaddr)exceptionHandler;
    
    /* Set Exception Stack Pointer */
    passupvector->exception_stackPtr = KERNEL_STACK;
}

/* ========================================================================
 * Function: initializeSystemVariables
 *
 * Description: Initializes global system variables and device semaphores.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              None
 * ======================================================================== */
HIDDEN void initializeSystemVariables() {
    /* Initialize process management counters */
    processCount = 0;
    softBlockCount = 0;

    /* Initialize ready queues */
    readyQueueHigh = mkEmptyProcQ();
    readyQueueLow = mkEmptyProcQ();

    /* Initialize current process */
    currentProcess = mkEmptyProcQ();
    
    /* Initialize device semaphores */
    int i;
    for (i = 0; i < DEVICE_COUNT; i++) {
        deviceSemaphores[i] = 0;
    }
}

/* ========================================================================
 * Function: createFirstProcess
 *
 * Description: Creates and initializes the first process in the system.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              Pointer to the first process, or NULL if allocation failed
 * ======================================================================== */
HIDDEN pcb_PTR createFirstProcess() {
    /* Allocate a PCB for the first process */
    pcb_PTR firstProcess = allocPcb();
    
    /* Check if allocation succeeded */
    if (firstProcess != mkEmptyProcQ()) {
        /* Set up first process state */
        /* allocPcb() already initialized all process fields to 0 */
        
        /* Set Program Counter to test function */
        firstProcess->p_s.s_pc = (memaddr)test;
        
        /* Set t9 to test function (required for MIPS calling convention) */
        firstProcess->p_s.s_t9 = (memaddr)test;
        
        /* Enable interrupts, set kernel mode, and enable timer */
        firstProcess->p_s.s_status = ALLOFF | STATUS_IEc | CAUSE_IP_MASK | STATUS_TE;
        
        /* Set stack pointer to top of RAM */
        firstProcess->p_s.s_sp = RAMTOP;
        
        /* Add to high priority ready queue and update process count */
        insertProcQ(&readyQueueHigh, firstProcess);
        processCount++;
    }
    
    return firstProcess;
}

