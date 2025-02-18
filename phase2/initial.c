/* initial.c */

/******************************************************************
* Module Level: Implements main() and exports the Nucleus's
* global variables.
******************************************************************/

#include "../h/initial.h"

/* Global variables */
extern int processCount;
extern int softBlockCount;
extern pcb_PTR readyQueue;
extern pcb_PTR currentProcess;
extern int deviceSemaphore[DEVICE_COUNT];   /* ASK PROF */

void main() {
  /* Initialize the processor 0 pass up vector */
  passupvector_t *passUpVector = (passupvector_t *)PASSUPVECTOR;
  passUpVector->tlb_refll_handler = (memaddr)uTLB_RefillHandler;
  passUpVector->tlb_refll_stackPtr = KERNEL_STACK;
  passUpVector->execption_handler = (memaddr)exceptionHandler;
  passUpVector->exception_stackPtr = KERNEL_STACK;

  /* Initialize Phase 1 data structures */
  initPcbs();
  initASL();

  /* Initialize Nucleus-maintained variables */
  processCount = 0;
  softBlockCount = 0;
  readyQueue = mkEmptyProcQ();
  currentProcess = NULL;

  /* Initialize device semaphores */
  for (int i = 0; i < DEVICE_COUNT; i++) {
    deviceSemaphore[i] = 0;
  }
  
  /* **Load the system-wide Interval Timer** */
  // Load Interval Timer with 100ms
  LDIT(PSEUDO_TIMER);

  // Create the first process
  pcb_t *firstProc = allocPcb();
  if (firstProc == NULL) {
      PANIC();
  }

  /* Initialize the first process state */
  /* IDK */

  /* Add the first process to the Ready Queue (Change to MLFQ) */
  insertProcQ(&readyQueue, firstProc);
  processCount++;

  /* Call the Scheduler */
  scheduler();
}
