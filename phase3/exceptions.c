/******************************* exceptions.c *************************************
 *
 * Module: Exception Handling
 *
 * Description:
 * This module handles all processor exceptions including system calls, TLB 
 * management, and program traps. It implements the "Pass Up or Die" philosophy
 * for handling exceptions that cannot be managed in the kernel.
 *
 * Implementation:
 * The module routes incoming exceptions to their appropriate handlers based 
 * on the exception code. It implements eight system calls (SYS1-SYS8),
 * TLB exception handling, and "Pass Up or Die" mechanisms. System call 
 * services include process management, synchronization, I/O, and time 
 * management functions.
 * 
 * Time Policy:
 * The module updates the current process's CPU time and stores the latest
 * process state from the BIOS data page before handling SYSCALLs (for blocking
 * calls). For non-blocking calls, the CPU time is updated again after the call to
 * account for the time spent in the nucleus.
 * 
 * Functions:
 * - exceptionHandler: Main exception handler that routes exceptions to
 *                      appropriate handlers.
 * - syscallHandler: Handles system call exceptions by dispatching to appropriate
 *                      system service based on the system call number in a0.
 * - createProcess: Implements SYS1 (CREATEPROCESS) system call. Creates a new
 *                      process with state provided by the caller.
 * - passeren: Implements SYS2 (PASSEREN) system call. Passes the current
 *                      process to a semaphore, blocking the current process.
 * - waitClock: Implements SYS3 (WAITCLOCK) system call. Blocks the current
 *                      process until the next clock tick.
 * - getCpuTime: Implements SYS4 (GETCPUTIME) system call. Returns the current
 *                      process's CPU time.
 * - waitIO: Implements SYS5 (WAITIO) system call. Blocks the current process
 *                      until an I/O operation completes.
 * - getSupportPtr: Implements SYS6 (GETSUPPORTPTR) system call. Returns the
 *                      support structure pointer for the current process.
 * - tlbExceptionHandler: Handles TLB-related exceptions by implementing the
 *                      Pass Up or Die approach.
 * - programTrapHandler: Handles program trap exceptions by implementing the
 *                      Pass Up or Die approach.
 * - passUpOrDie: Implements the Pass Up or Die principle. Either passes the
 *                      exception to the support level or terminates the process.
 * - copyState: Helper function to make a deep copy of a processor state.
 * - updateCurrentProcess: Updates the current process state from the exception
 *                      state and calculates remaining time quantum.
 * - updateProcessTime: Updates the CPU time for the current process based on
 *                      elapsed time since last update.
 *
 * Written by Aryah Rao and Anish Reddy
 *
 ***************************************************************************/

/******************** Included Header Files ********************/
#include "../h/exceptions.h"

/******************** Function Definitions ********************/

/* ========================================================================
 * Function: exceptionHandler
 *
 * Description: Main exception handler that routes exceptions to appropriate
 *              handlers based on the exception code in the Cause register.
 * 
 * Parameters:
 *              None (accesses exception state from BIOS data page)
 * 
 * Returns:
 *              Does not return (control passes to specific handlers)
 * ======================================================================== */
void exceptionHandler() {
    /* Get exception state from the BIOS data page */
    state_PTR exceptionState = (state_PTR )BIOSDATAPAGE;
    
    /* Extract exception code from the Cause register */
    unsigned int cause = exceptionState->s_cause;
    int excCode = (cause & CAUSE_EXCCODE_MASK) >> CAUSE_EXCCODE_SHIFT;

    /* Dispatch to the appropriate exception handler based on exception code */
    switch (excCode) {
        case INTERRUPTS:
            /* Hardware interrupts */
            interruptHandler();
            break;
            
        case TLBMOD:
        case TLBINVLDL:
        case TLBINVLDS:
            /* TLB-related exceptions */
            tlbExceptionHandler();
            break;
            
        case SYSCALLS:
            /* System calls */
            syscallHandler();
            break;
            
        case ADDRINVLD:
        case ADDRINVLDS:
        case BUSINVLD:
        case BUSINVLDL:
        case BREAKPOINT:
        case RESERVEDINST:
        case COPROCUNUSABLE:
        case ARITHOVERFLOW:
            /* Program traps */
            programTrapHandler();
            break;
            
        default:
            /* Unknown exception code - critical error */
            PANIC();
    }
    
    /* Control should not reach here as each handler either returns to
     * user mode or calls the scheduler */
}

/* ========================================================================
 * Function: syscallHandler
 *
 * Description: Handles system call exceptions by dispatching to appropriate
 *              system service based on the system call number in a0.
 * 
 * Parameters:
 *              None (accesses exception state from BIOS data page)
 * 
 * Returns:
 *              Does not return (transfers control to service or scheduler)
 * ======================================================================== */
void syscallHandler() {
    /* Get exception state from BIOS data page */
    state_PTR exceptionState = (state_PTR )BIOSDATAPAGE;

    /* Increment PC before handling syscall to point to next instruction */
    exceptionState->s_pc += WORDLEN;

    /* If Syscall greater than 8, then pass up the exception */
    if (exceptionState->s_a0 > 8) {
        passUpOrDie(GENERALEXCEPT);
        return;
    }

    /* Check if the system call was issued from user mode (KUp on) */
    if ((exceptionState->s_status & STATUS_KUp) != ALLOFF) {
        /* User mode system call attempt - convert to Reserved Instruction exception */
        exceptionState->s_cause = (exceptionState->s_cause & ~CAUSE_EXCCODE_MASK) 
                                 | (RESERVEDINST << CAUSE_EXCCODE_SHIFT);

        /* Handle as program trap */
        programTrapHandler();
        return;
    }

    /* Update current process state and get remaining time quantum */
    int quantumLeft = updateCurrentProcess(exceptionState);

    /* Dispatch to appropriate system call service based on system call number in a0 */
    switch (exceptionState->s_a0) {
        case CREATEPROCESS:
            /* SYS1: Create a new process */
            createProcess();
            break;
            
        case TERMINATEPROCESS:
            /* SYS2: Terminate process (and its descendants) */
            if (currentProcess != mkEmptyProcQ()) {
                terminateProcess(mkEmptyProcQ());  /* NULL means terminate current process */
            }
            break;
            
        case PASSEREN:
            /* SYS3: P operation (wait) on a semaphore */
            passeren((int *)currentProcess->p_s.s_a1);
            break;
            
        case VERHOGEN:
            /* SYS4: V operation (signal) on a semaphore */
            verhogen((int *)currentProcess->p_s.s_a1);
            break;
            
        case WAITIO:
            /* SYS5: Wait for I/O completion */
            waitIO();
            break;
            
        case GETCPUTIME:
            /* SYS6: Get CPU time used by current process */
            getCpuTime();
            break;
            
        case WAITCLOCK:
            /* SYS7: Wait for clock tick */
            waitClock();
            break;
            
        case GETSUPPORTPTR:
            /* SYS8: Get support structure pointer */
            getSupportPtr();
            break;
            
        default:
            /* Invalid system call number - pass to support level or terminate */
            passUpOrDie(GENERALEXCEPT);
    }

    /* Resume execution of current process if it is still runnable */
    if (currentProcess != mkEmptyProcQ()) {
        /* Update process time to account for kernel execution */
        updateProcessTime();
        /* Load process state back into CPU */
        loadProcessState(&currentProcess->p_s, quantumLeft);
    }
    
    /* Current process was blocked or terminated - call scheduler */
    scheduler();
}

/* ========================================================================
 * Function: createProcess
 *
 * Description: Implements SYS1 (CREATEPROCESS) system call. Creates a new
 *              process with state provided by the caller.
 * 
 * Parameters:
 *              None (uses current process state for arguments)
 * 
 * Returns:
 *              None
 * ======================================================================== */
void createProcess() {
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Allocate a new PCB */
    pcb_PTR newPCB = allocPcb();

    if (newPCB == mkEmptyProcQ()) {
        /* No free PCBs - return error code */
        currentProcess->p_s.s_v0 = ERROR;
    } else {
        /* Copy state from address in a1 to new PCB */
        copyState(&newPCB->p_s, (state_PTR )currentProcess->p_s.s_a1);

        /* Set up support structure if provided in a2 */
        if (currentProcess->p_s.s_a2 != 0) {
            newPCB->p_supportStruct = (support_t *)currentProcess->p_s.s_a2;
        } else {
            newPCB->p_supportStruct = NULL;
        }

        /* Set parent and add to parent's child list */
        newPCB->p_prnt = currentProcess;
        insertChild(currentProcess, newPCB);

        /* Add to high priority ready queue */
        insertProcQ(&readyQueueHigh, newPCB);

        /* Update system process count */
        processCount++;

        /* Return success code */
        currentProcess->p_s.s_v0 = 0;
    }

    /* Control is returned to syscallHandler, which will either
    * resume the current process or call the scheduler as needed */
}

/* ========================================================================
 * Function: terminateProcess
 *
 * Description: Implements SYS2 (TERMINATEPROCESS) system call. Terminates
 *              a process and all its descendents recursively.
 * 
 * Parameters:
 *              process - Pointer to the process to terminate, or NULL for
 *                        the current process
 * 
 * Returns:
 *              None
 * ======================================================================== */
void terminateProcess(pcb_PTR process) {
    if (process == mkEmptyProcQ()) {
        process = currentProcess;
    }

    /* Recursively terminate all children */
    while (!emptyChild(process)) {
        terminateProcess(removeChild(process));
    }
    
    /* Remove from parent's child list */
    if (process->p_prnt != mkEmptyProcQ()) {
        outChild(process);
    }

    /* Remove from appropriate queue based on process state */
    if (process->p_semAdd != NULL) {
        /* Process is blocked on a semaphore */
        int *semAdd = process->p_semAdd;
        pcb_PTR removedProcess = outBlocked(process);
        
        /* Update process count if successfully removed */
        if (removedProcess != mkEmptyProcQ()) {
            processCount--;
        }
        
        /* Adjust semaphore or soft block count based on semaphore type */
        if ((&deviceSemaphores[0] <= semAdd) && (semAdd <= &deviceSemaphores[DEVICE_COUNT])) {
            /* Device semaphore */
            softBlockCount--;
        } else {
            /* Regular semaphore */
            (*semAdd)++;
        }
    } else {
        /* Process is in ready queue */
        pcb_PTR removedProcess = getProcess(process);
        
        /* Update process count if successfully removed */
        if (removedProcess != mkEmptyProcQ()) {
            processCount--;
        }
    }

    /* Special handling if terminating current process */
    if (process == currentProcess) {
        currentProcess = mkEmptyProcQ();
        processCount--;
        freePcb(process);
        scheduler();
    }

    /* Return PCB to free list */
    freePcb(process);
}

/* ========================================================================
 * Function: passeren
 *
 * Description: Implements SYS3 (PASSEREN) system call. Performs P operation
 *              on a semaphore, potentially blocking the current process.
 * 
 * Parameters:
 *              semAdd - Address of the semaphore to operate on
 * 
 * Returns:
 *              None
 * ======================================================================== */
void passeren(int *semAdd) {
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Decrement semaphore value */
    (*semAdd)--;

    /* Check if process should block */
    if (*semAdd < 0) {
        /* Update process time before blocking */
        updateProcessTime();

        /* Block process on semaphore */
        insertBlocked(semAdd, currentProcess);

        /* Current process is now blocked */
        currentProcess = mkEmptyProcQ();

        /* Select new process to run */
        scheduler();
    }

    /* Control is returned to syscallHandler, which will either
    * resume the current process or call the scheduler as needed */
}

/* ========================================================================
 * Function: verhogen
 *
 * Description: Implements SYS4 (VERHOGEN) system call. Performs V operation
 *              on a semaphore, potentially unblocking a waiting process.
 * 
 * Parameters:
 *              semAdd - Address of the semaphore to operate on
 * 
 * Returns:
 *              Pointer to unblocked process if any, NULL otherwise
 * ======================================================================== */
pcb_PTR verhogen(int *semAdd) {
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Increment semaphore value */
    (*semAdd)++;

    /* Check if any process should be unblocked */
    pcb_PTR p = mkEmptyProcQ();
    if (*semAdd <= 0) {
        /* Remove a process from the semaphore's blocked queue */
        p = removeBlocked(semAdd);

        if (p != mkEmptyProcQ()) {
            /* Add unblocked process to high priority ready queue */
            insertProcQ(&readyQueueHigh, p);
        }
    }

    return p;
    
    /* Control is returned to syscallHandler, which will either
    * resume the current process or call the scheduler as needed */
}

/* ========================================================================
 * Function: waitIO
 *
 * Description: Implements SYS5 (WAITIO) system call. Blocks the current
 *              process until an I/O operation completes.
 * 
 * Parameters:
 *              None (uses current process state for arguments)
 * 
 * Returns:
 *              None
 * ======================================================================== */
void waitIO() {
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Get device parameters from registers */
    int line = currentProcess->p_s.s_a1;
    int device = currentProcess->p_s.s_a2;
    int term_read = currentProcess->p_s.s_a3;  /* For terminals: 1 for read, 0 for write */
    
    /* Calculate device semaphore index */
    int devSemaphoreIndex = DEVPERINT * (line - DISKINT) + device;
    if (line == TERMINT && term_read) {
        /* Terminal read operations use a different semaphore */
        devSemaphoreIndex += DEVPERINT;
    }
    
    /* Increment soft block count for this I/O operation */
    softBlockCount++;

    /* Block the process on the device semaphore */
    passeren(&deviceSemaphores[devSemaphoreIndex]);

    /* Control is returned to syscallHandler, which will either
    * resume the current process or call the scheduler as needed */
}

/* ========================================================================
 * Function: getCpuTime
 *
 * Description: Implements SYS6 (GETCPUTIME) system call. Returns the CPU
 *              time used by the current process.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              None (places result in v0 register)
 * ======================================================================== */
void getCpuTime() {
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Update time to account for kernel execution */
    updateProcessTime();

    /* Store CPU time in v0 register for return to caller */
    currentProcess->p_s.s_v0 = currentProcess->p_time;

    /* Control is returned to syscallHandler, which will either
    * resume the current process or call the scheduler as needed */
}

/* ========================================================================
 * Function: waitClock
 *
 * Description: Implements SYS7 (WAITCLOCK) system call. Blocks the current
 *              process until the next clock tick.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              None
 * ======================================================================== */
void waitClock() {
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Increment soft block count */
    softBlockCount++;

    /* Block on pseudoclock semaphore */
    passeren(&deviceSemaphores[DEVICE_COUNT-1]);

    /* Control is returned to syscallHandler, which will either
    * resume the current process or call the scheduler as needed */
}

/* ========================================================================
 * Function: getSupportPtr
 *
 * Description: Implements SYS8 (GETSUPPORTPTR) system call. Returns a pointer
 *              to the support structure for the current process.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              None (places result in v0 register)
 * ======================================================================== */
void getSupportPtr() {
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Return support structure pointer in v0 */
    currentProcess->p_s.s_v0 = (int)currentProcess->p_supportStruct;

    /* Control is returned to syscallHandler, which will either
    * resume the current process or call the scheduler as needed */
}

/* ========================================================================
 * Function: tlbExceptionHandler
 *
 * Description: Handles TLB-related exceptions by implementing the Pass Up
 *              or Die approach.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              Does not return (passes exception up or terminates process)
 * ======================================================================== */
void tlbExceptionHandler() {
    /* TLB exceptions are passed to the support level or cause termination */
    passUpOrDie(PGFAULTEXCEPT);
}

/* ========================================================================
 * Function: programTrapHandler
 *
 * Description: Handles program trap exceptions by implementing the Pass Up
 *              or Die approach.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              Does not return (passes exception up or terminates process)
 * ======================================================================== */
void programTrapHandler() {
    /* Program traps are passed to the support level or cause termination */
    passUpOrDie(GENERALEXCEPT);
}

/* ========================================================================
 * Function: passUpOrDie
 *
 * Description: Implements the Pass Up or Die principle. Either passes the
 *              exception to the support level or terminates the process.
 * 
 * Parameters:
 *              exceptionType - Type of exception (PGFAULTEXCEPT or GENERALEXCEPT)
 * 
 * Returns:
 *              Does not return (passes control to support level or terminates)
 * ======================================================================== */
void passUpOrDie(int exceptionType) {
    /* Check if current process has a support structure */
    if (currentProcess != mkEmptyProcQ() && currentProcess->p_supportStruct != NULL) {
        /* Process has support structure - pass exception to the support level */
        
        /* Copy exception state to the appropriate field in the support structure */
        copyState(&currentProcess->p_supportStruct->sup_exceptState[exceptionType], 
                 (state_PTR)BIOSDATAPAGE);

        /* Load support level context to handle the exception */
        LDCXT(currentProcess->p_supportStruct->sup_exceptContext[exceptionType].c_stackPtr,
              currentProcess->p_supportStruct->sup_exceptContext[exceptionType].c_status,
              currentProcess->p_supportStruct->sup_exceptContext[exceptionType].c_pc);
    } else {
        /* Process has no support structure - terminate it */
        terminateProcess(currentProcess);
    }
}

/* ========================================================================
 * Function: copyState
 *
 * Description: Helper function to make a deep copy of a processor state.
 * 
 * Parameters:
 *              dest - Destination state structure
 *              src - Source state structure
 * 
 * Returns:
 *              None
 * ======================================================================== */
void copyState(state_PTR dest, state_PTR src) {
    /* Check for NULL pointers */
    if (dest == NULL || src == NULL) {
        return;
    }

    /* Copy special registers */
    dest->s_entryHI = src->s_entryHI;
    dest->s_cause = src->s_cause;
    dest->s_status = src->s_status;
    dest->s_pc = src->s_pc;
    
    /* Copy general purpose registers */
    int i;
    for (i = 0; i < STATEREGNUM; i++) {
        dest->s_reg[i] = src->s_reg[i];
    }
}

/* ========================================================================
 * Function: updateCurrentProcess
 *
 * Description: Updates the current process state from the exception state
 *              and calculates remaining time quantum.
 * 
 * Parameters:
 *              exceptionState - Pointer to exception state from BIOS data page
 * 
 * Returns:
 *              Remaining time quantum, or 0 if no current process
 * ======================================================================== */
int updateCurrentProcess(state_PTR exceptionState) {
    /* Check if current process exists */
    if (currentProcess != mkEmptyProcQ()) {
        /* Save quantum left */
        int quantumLeft = getTIMER();

        /* Update process state with exception state */
        copyState(&currentProcess->p_s, exceptionState);

        /* Update CPU time for the current process */
        updateProcessTime();

        return quantumLeft;
    }
    
    return 0; /* No current process */
}

/* ========================================================================
 * Function: updateProcessTime
 *
 * Description: Updates the CPU time for the current process based on elapsed
 *              time since last update.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              None
 * ======================================================================== */
void updateProcessTime() {
    /* Check if current process exists */
    if (currentProcess != mkEmptyProcQ()) {
        /* Calculate elapsed time since last measurement */
        cpu_t currentTOD;
        STCK(currentTOD);
        
        /* Add elapsed time to process's accumulated CPU time */
        currentProcess->p_time += (currentTOD - startTOD);

        /* Update start time for next measurement */
        startTOD = currentTOD;
    }
}
