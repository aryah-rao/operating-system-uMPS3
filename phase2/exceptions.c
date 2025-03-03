/**************************** exceptions.c **********************************
 *
 * The exception handling module for the PandOS Operating System.
 * 
 * Policy:
 * - Handles all exceptions and system calls.
 * - Implements "Pass Up or Die" logic for exceptions.
 * - Implements TLB-Refill event handler.
 *  
 *
 * Functions:
 * - exceptionHandler: Main exception handler
 * - syscallHandler: System call handler
 * - createProcess: Create process system call
 * - terminateProcess: Terminate process system call
 * - passeren: P operation system call
 * - verhogen: V operation system call
 * - waitIO: Wait for I/O system call
 * - getCpuTime: Get CPU time system call
 * - waitClock: Wait for clock system call
 * - getSupportPtr: Get support pointer system call
 * - copyState: Copy state function
 * - uTLB_RefillHandler: TLB-Refill event handler
 * - tlbExceptionHandler: TLB exception handler
 * - programTrapHandler: Program Trap handler
 * - passUpOrDie: Pass up or die function
 *
 *********************************************************************/

#include "../h/exceptions.h"


/* DONE */
void exceptionHandler() {
    /* *
    * This is the general exception handler for the Nucleus. It is called
    * by the BIOS after an exception occurs. It saves the state of the
    *  process and then dispatches to the appropriate handler based on the
    * cause of the exception.
    */

    state_t *exceptionState = (state_t *)BIOSDATAPAGE; /* Address: 0x0FFFF000 */
    unsigned int cause = exceptionState->s_cause; /* reads the cause */
    int excCode = (cause & CAUSE_EXCCODE_MASK) >> CAUSE_EXCCODE_SHIFT; /* gets excCode ( cause & 0x0000007C >> 2) */

    /* Dispatch to appropriate exception handler */
    switch (excCode) {
        case INTERRUPTS:                        /* 0: Interrupts */
            interruptHandler(); /* Call Interrupt Handler */
            break;
        case TLBMOD:                            /* 1: TLB-Modification Exception */
        case TLBINVLD:                          /* 2: TLB Invalid Exception */
        case TLBINVLDL:                         /* 3: TLB Invalid Exception on a Store instr */
            tlbExceptionHandler(); /* Call TLB Exception Handler */
        break;
        case SYSCALL_EXCEPTION:                 /* 8: System Call */
            syscallHandler(); /* Call SYSCALL Handler */
        break;
        case ADDRINVLD:                         /* 4: Address Error Exception: on a Load or instruction fetch */
        case ADDRINVLDS:                        /* 5: Address Error Exception: on a Store instr */
        case BUSINVLD:                          /* 6: Bus Error Exception: on an instruction fetch */
        case BUSINVLDL:                         /* 7: Bus Error Exception: on a Load/Store data access */
        case BREAKPOINT:                        /* 9: Breakpoint Exception */
        case RESERVEDINST:                      /* 10: Reserved Instruction Exception*/
        case COPROCUNUSABLE:                    /* 11: Coprocessor Unusable Exception */
        case ARITHOVERFLOW:                     /* 12: Arithmetic Overflow Exception */
            programTrapHandler(); /* Call Program Trap Handler */
            break;
        default:                                /* Anything else: Unexpected exception code */
            PANIC(); /* PANIC!! */
    }
}

/* PARTIALLY DONE */
void syscallHandler() {
    /* Get exception state */
    state_t *exceptionState = (state_t *)BIOSDATAPAGE; /* Address: 0x0FFFF000 */

    /* Increment PC before handling syscall */
    exceptionState->s_pc += WORDLEN;

    /* User mode check */
    if ((exceptionState->s_status & STATUS_KUp) != ALLOFF) { /* s_status & 0x00000008 */

        /* Should setting CauseExcCode in the stored exception state to RI (Reserved Instruction)*/ /* DOUBLE CHECK */
        exceptionState->s_cause = (exceptionState->s_cause & ~CAUSE_EXCCODE_MASK) | (RESERVEDINST << CAUSE_EXCCODE_SHIFT);

        /* Program Trap Exception */
        programTrapHandler(exceptionState);
        return;
    }

    /* Update current process state */
    int quantumLeft = updateCurrentProcess(exceptionState);

    /* Handle syscalls */
    switch (exceptionState->s_a0) {
        case CREATEPROCESS:                                     /* 1: Create Process (SYS1) */
            createProcess();
            break;
        case TERMINATEPROCESS:                                  /* 2: Terminate Process (SYS2) */
            if (currentProcess!= NULL) {
                terminateProcess(NULL);  /* Call terminateProcess helper function, NULL means terminate current process */
            }
            break;
        case PASSEREN:                                          /* 3: Passeren (SYS3) */
            passeren((int *)currentProcess->p_s.s_a1); /* Get semaphore address from a1 */
            break;
        case VERHOGEN:                                          /* 4: Verhogen (SYS4) */
            verhogen((int *)currentProcess->p_s.s_a1); /* Get semaphore address from a1 */
            break;
        case WAITIO:                                            /* 5: Wait I/O (SYS5) */
            waitIO();
            break;
        case GETCPUTIME:                                        /* 6: Get CPU Time (SYS6) */
            getCpuTime();
            break;
        case WAITCLOCK:                                         /* 7: Wait Clock (SYS7) */
            waitClock();
            break;
        case GETSUPPORTPTR:                                     /* 8: Get Support Pointer (SYS8) */
            getSupportPtr();
            break;
        default:                                                /* Anything else: Invalid syscall */
            passUpOrDie(GENERALEXCEPT); /* GENERALEXCEPT for PassUpOrDie */
    }

    /* Reload updated process state */
    if (currentProcess != NULL) {
        loadProcessState(&currentProcess->p_s, quantumLeft); /* Load process state */
    }
    /* If current process is NULL, call scheduler */
    scheduler();
}

/*********************SYSCALL SERVICE IMPLEMENTATIONS**********************/

/* DONE */
void createProcess() { /* SYS1 */
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Allocate a new PCB */
    pcb_PTR newPCB = allocPcb(); /* allocPcb() initializes all fields of the new PCB */

    /* Check if allocation was successful */
    if (newPCB == NULL) {
        currentProcess->p_s.s_v0 = -1; /* Return -1 in v0 */

    } else {
        /* Copy state structure from exception state to new PCB */
        copyState(&newPCB->p_s, (state_t *)currentProcess->p_s.s_a1);

        /* Copy support structure */
        if (currentProcess->p_s.s_a2 != 0) {
            newPCB->p_supportStruct = (support_t *)currentProcess->p_s.s_a2;
        } else {
            newPCB->p_supportStruct = NULL;
        }

        /* Set parent */
        newPCB->p_prnt = currentProcess;

        /* Insert into parent process's child list */
        insertChild(currentProcess, newPCB);

        /* Insert into ready queue */
        insertProcQ(&readyQueue, newPCB);

        /* Increment process count */
        processCount++;

        /* Return 0 in v0 */
        currentProcess->p_s.s_v0 = 0;
    }

    /* Control is returned to syscallHandler, which will either
     * resume the current process or call the scheduler as needed */
}

/* DONE */
void terminateProcess(pcb_PTR process) {
    if (process == NULL) 
        process = currentProcess;

    /* Recursively terminate all children */
    while (!emptyChild(process)) {
        terminateProcess(removeChild(process));
    }
    
    /* Remove from parent */
    if (process->p_prnt != NULL) {
        outChild(process);
    }

    /* If blocked on ASL, remove from semaphore queue */
    if (process->p_semAdd != NULL) {
        int *semAdd = process->p_semAdd;

        pcb_PTR removedProcess = outBlocked(process);

        /* Decrement process count */
        if (removedProcess != NULL) {
            processCount--;
        }
        
        /* If device semaphore, decrement softBlockCount */
        if ((semAdd >= &deviceSemaphores[0]) &&
            (semAdd < &deviceSemaphores[DEVICE_COUNT])) {
            softBlockCount--;
        } else {
            (*semAdd)++;    /* Not a device semaphore, so increment */
        }
    } else {
        /* Remove from ready queue */
        pcb_PTR removedProcess = outProcQ(&readyQueue, process);

        /* Decrement process count */
        if (removedProcess != NULL) {
            processCount--;
        }
    }

    /* If terminating current process, call scheduler */
    if (process == currentProcess) {

        /* Set current process to NULL */
        currentProcess = mkEmptyProcQ();

        /* Decrement process count */
        processCount--;

        /* Free PCB and update process count */
        freePcb(process);
        
        /* Call scheduler */
        scheduler();
    }

    /* Free PCB and update process count */
    freePcb(process);
}

/* DONE */
void passeren(int *semAdd) { /* SYS3 */
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Decrement integer at semaphore address */
    (*semAdd)--;

    /* If semaphore is negative, block the process */
    if (*semAdd < 0) {

        /* Block the process */
        insertBlocked(semAdd, currentProcess);

        /* Increment softBlockCount */
        softBlockCount++;

        /* Set current process to NULL */
        currentProcess = NULL;

        /* Call scheduler */
        scheduler();
    }

    /* Control is returned to syscallHandler, which will either
     * resume the current process or call the scheduler as needed */
}

/* DONE */
pcb_PTR verhogen(int *semAdd) { /* SYS4 */
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Increment semaphore address */
    (*semAdd)++;

    /* If semaphore is non-positive, unblock process */
    pcb_PTR p;
    if (*semAdd <= 0) {

        /* Unblock process */
        p = removeBlocked(semAdd);

        if (p != NULL) {

            /* Insert unblocked process into ready queue */
            insertProcQ(&readyQueue, p);

            /* Decrement softBlockCount */
            softBlockCount--;
        }
    }

    return p;

    /* Control is returned to syscallHandler, which will either
     * resume the current process or call the scheduler as needed */
}

/* DONE */
void waitIO() { /* SYS5 */
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Get line and device number from a1 and a2 */
    int line = currentProcess->p_s.s_a1;
    int device = currentProcess->p_s.s_a2;
    int term_read = currentProcess->p_s.s_a3;  /* For terminals: 1 for read, 0 for write */
    
    /* Calculate device semaphore index */
    int dev_sem_index;
    if (line == TERMINT && term_read)
        dev_sem_index = DEVPERINT * (line - DISKINT) + device + DEVPERINT;
    else
        dev_sem_index = DEVPERINT * (line - DISKINT) + device;
    
    /* Perform P operation using passeren */
    passeren(&deviceSemaphores[dev_sem_index]);

    /* Control is returned to syscallHandler, which will either
     * resume the current process or call the scheduler as needed */
}

/* DONE */
void getCpuTime() { /* SYS6 */
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */
    
    /* Store cpuTime in s_v0 */
    currentProcess->p_s.s_v0 = currentProcess->p_time;
    
    /* Control is returned to syscallHandler, which will either
     * resume the current process or call the scheduler as needed */
}

/* DONE */
void waitClock() { /* SYS7 */
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Get pseudo-clock semaphore index */
    int psem = DEVICE_COUNT; /* Pseudo-clock semaphore index */
    
    /* Perform P operation using passeren */
    passeren(&deviceSemaphores[psem]);

    /* Control is returned to syscallHandler, which will either
     * resume the current process or call the scheduler as needed */
}

/* DONE */
void getSupportPtr() { /* SYS8 */
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Return support structure pointer in v0 */
    currentProcess->p_s.s_v0 = (int)currentProcess->p_supportStruct;
    
    /* Control is returned to syscallHandler, which will either
     * resume the current process or call the scheduler as needed */
}

/* DONE */
void uTLB_RefillHandler() {
    /* *
    * Provided skeleton TLB-Refill event handler.
    */
    setENTRYHI(KUSEG);
    setENTRYLO(KSEG0);
    TLBWR();
    LDST((state_PTR)BIOSDATAPAGE);
}
 
/* DONE */
void passUpOrDie(int exceptionType) {
    /* *
    * Implements the "Pass Up or Die" logic. If the
    * current process has a Support Structure, the exception is passed up to
    * the Support Level. Otherwise, the process is terminated.
    */
    /* Check if current process has a support structure */
    if (currentProcess != NULL && currentProcess->p_supportStruct != NULL) {
        /* *Pass up to the Support Level */

        /* *Copy the saved exception state from the BIOS Data Page to the
        * correct sup_exceptState field of the Current Process. */
        copyState(&currentProcess->p_supportStruct->sup_exceptState[exceptionType], (state_PTR)BIOSDATAPAGE);

        /* *Perform a LDCXT using the fields from the correct
        * sup_exceptContext field of the Current Proces.*/
        LDCXT(
            currentProcess->p_supportStruct->sup_exceptContext[exceptionType].c_stackPtr,
            currentProcess->p_supportStruct->sup_exceptContext[exceptionType].c_status,
            currentProcess->p_supportStruct->sup_exceptContext[exceptionType].c_pc);
    } else {
        /* *No support structure, terminate the process */
        terminateProcess(currentProcess);

        /* terminateProcess calls scheduler */
    }
}

/* DONE */
void tlbExceptionHandler() {
    /* Handles TLB exceptions: Passes it up to the Support Level */
    passUpOrDie(PGFAULTEXCEPT);
}

/* DONE */
void programTrapHandler() {
    /* *
    * Handles Program Trap exceptions Passes it up to the Support Level */
    passUpOrDie(GENERALEXCEPT);
}

/* DONE */
/* function to perform deep copy of state */
void copyState(state_t *dest, state_t *src) {
    /* Check if destination and source states are not NULL */
    if (dest == NULL || src == NULL) {
        return;
    }

    /* Copy state fields */
    dest->s_entryHI = src->s_entryHI;
    dest->s_cause = src->s_cause;
    dest->s_status = src->s_status;
    dest->s_pc = src->s_pc;
    
    /* Copy all registers */
    int i;
    for (i = 0; i < STATEREGNUM; i++) {
        dest->s_reg[i] = src->s_reg[i];
    }
}


int updateCurrentProcess(state_t *exceptionState) {
    /* Check if current process is NULL */
    if (currentProcess != NULL) {
        /* Variables */
        int currentTOD;
        int quantumLeft;

        /* Update current process state with exception state */
        copyState(&currentProcess->p_s, exceptionState);

        /* Update CPU time */
        STCK(currentTOD);
        currentProcess->p_time += (currentTOD - startTOD);

        /* Calculate quantum left before interrupt occurred */
        quantumLeft = getTIMER();

        return quantumLeft;
    } else {
        return 0; /* No current process, return 0 */
    }
}