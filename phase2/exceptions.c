/* exceptions.c */

/******************************************************************
 * Module Level: Implements the TLB, Program Trap, and SYSCALL
 * exception handlers. It also contains the skeleton TLB-Refill event handler.
 ******************************************************************/

#include "../h/exceptions.h"

/* Current TOD */
HIDDEN cpu_t currentTime;

/* DONE */
void exceptionHandler() {
    /* *
    * This is the general exception handler for the Nucleus [4]. It is called
    * by the BIOS after an exception occurs [5]. It saves the state of the
    *  process and then dispatches to the appropriate handler based on the
    * cause of the exception [6].
    */

    state_t *exceptionState = (state_t *)BIOSDATAPAGE; /* Address: 0x0FFF.F000 */
    unsigned int cause = exceptionState->s_cause; /* reads the cause [3] */
    int excCode = (cause & CAUSE_EXCCODE_MASK) >> CAUSE_EXCCODE_SHIFT; /* gets excCode ( cause & 0x0000007C >> 2) */
    
    switch (excCode) {
        case INTERRUPTS: /* 0: Interrupts */
            interruptHandler(); /* Call Interrupt Handler */
            break;
        case TLBMOD: /* 1: TLB-Modification Exception */
        case TLBINVLD: /* 2: TLB Invalid Exception */
        case TLBINVLDL: /* 3: TLB Invalid Exception on a Store instr */
            tlbExceptionHandler(exceptionState); /* Call TLB Exception Handler */
        break;
        case SYSCALL_EXCEPTION: /* 8: System Call */
            syscallHandler(exceptionState); /* Call SYSCALL Handler */
        break;
        case ADDRINVLD: /* 4: Address Error Exception: on a Load or instruction fetch */
        case ADDRINVLDS: /* 5: Address Error Exception: on a Store instr */
        case BUSINVLD: /* 6: Bus Error Exception: on an instruction fetch */
        case BUSINVLDL: /* 7: Bus Error Exception: on a Load/Store data access */
        case BREAKPOINT: /* 9: Breakpoint Exception */
        case RESERVEDINST: /* 10: Reserved Instruction Exception*/
        case COPROCUNUSABLE: /* 11: Coprocessor Unusable Exception */
        case ARITHOVERFLOW: /* 12: Arithmetic Overflow Exception */
            programTrapHandler(exceptionState); /* Call Program Trap Handler */
            break;
        default: /* Anything else: Unexpected exception code */
            PANIC(); /* PANIC!! */
    }
}

/* PARTIALLY DONE */
void syscallHandler(state_PTR exceptionState) {
    /* Increment PC before handling syscall */
    exceptionState->s_pc += WORDLEN;

    /* User mode check */
    if ((exceptionState->s_status & STATUS_KUp) != ALLOFF) { /* s_status & 0x00000008 */

        /* Should setting Cause.ExcCode in the stored exception state to RI (Reserved Instruction)*/ /* DOUBLE CHECK */
        exceptionState->s_cause = (exceptionState->s_cause & ~CAUSE_EXCCODE_MASK) | (RESERVEDINST << CAUSE_EXCCODE_SHIFT); /* Cause.ExcCode = 10 */

        /* Program Trap Exception */
        programTrapHandler(exceptionState);
        return;
    }

    /* Get currentTOD for blocking and cpuTime */
    STCK(currentTime);
    currentProcess->p_time += (currentTime - startTOD); /* Update CPU time */
    startTOD = currentTime; /* Update startTOD */

    /* Copy state to current process */
    copyState(&currentProcess->p_s, exceptionState);

    /* Handle syscalls */
    switch (exceptionState->s_a0) {
        case CREATEPROCESS: /* 1: Create Process (SYS1) */
            createProcess(exceptionState);  /* Call createProcess helper function */
            break;
        case TERMINATEPROCESS:  /* 2: Terminate Process (SYS2) */
            if (currentProcess!= NULL) {
                terminateProcess(currentProcess);  /* Call terminateProcess helper function, NULL means terminate current process */
            }
            break;
        case PASSEREN:  /* 3: Passeren (SYS3) */
            passeren(exceptionState);
            break;
        case VERHOGEN:  /* 4: Verhogen (SYS4) */
            verhogen(exceptionState);
            break;
        case WAITIO:    /* 5: Wait I/O (SYS5) */
            waitIO(exceptionState);
            break;
        case GETCPUTIME:    /* 6: Get CPU Time (SYS6) */
            getCpuTime(exceptionState);
            break;
        case WAITCLOCK:     /* 7: Wait Clock (SYS7) */
            waitClock(exceptionState);
            break;
        case GETSUPPORTPTR:   /* 8: Get Support Pointer (SYS8) */
            getSupportPtr(exceptionState);
            break;
        default:            /* Anything else: Invalid syscall */
            passUpOrDie(GENERALEXCEPT, exceptionState); /* PassUpOrDie */
    }
}

/*********************SYSCALL SERVICE IMPLEMENTATIONS**********************/

/* DONE */
void createProcess(state_PTR exceptionState) { /* SYS1 */
    /* Allocate a new PCB */
    pcb_PTR newPCB = allocPcb(); /* allocPcb() initializes all fields of the new PCB */

    /* Check if allocation was successful */
    if (newPCB == NULL) {
        exceptionState->s_v0 = -1; /* Return -1 in v0 */

    } else {
        /* Copy state structure from exception state to new PCB */
        copyState(&newPCB->p_s, (state_t *)exceptionState->s_a1);

        /* Copy support structure */
        if (exceptionState->s_a2 != 0) {
            newPCB->p_supportStruct = (support_t *)exceptionState->s_a2;
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
        exceptionState->s_v0 = 0;
    }
    
    /* Already incremented PC in syscallHandler */

    /* Reload updated process state */
    loadProcessState(exceptionState, 0);
}

/* DONE */
void terminateProcess(pcb_PTR p) {
    if (p == NULL) 
        p = currentProcess;

    /* Recursively terminate all children */
    while (p->p_child != NULL) {
        terminateProcess(removeChild(p));
    }
    
    /* Remove from parent */
    if (p->p_prnt != NULL) {
        outChild(p);
    }
    
    /* If blocked on semaphore, remove from semaphore queue */
    if (p->p_semAdd != NULL) {
        int *semAdd = p->p_semAdd;
        outBlocked(p);
        
        /* If device semaphore, decrement softBlockCount */
        if ((semAdd >= &deviceSemaphores[0]) &&
            (semAdd <= &deviceSemaphores[DEVICE_COUNT-1])) {
            softBlockCount--;
        }
    }
    
    /* Free PCB and update process count */
    freePcb(p);
    processCount--;
    
    /* If terminating current process, call scheduler */
    if (p == currentProcess) {
        currentProcess = NULL;
        scheduler();
    }
}

/* DONE */
void passeren(state_PTR exceptionState) { /* SYS3 */
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Get semaphore address from a1 */
    int *semAdd = (int *)currentProcess->p_s.s_a1;

    /* Decrement integer at semaphore address */
    (*semAdd)--;

    /* If semaphore is negative, block the process */
    if (*semAdd < 0) {
        /* Block the process */
        insertBlocked(semAdd, currentProcess);
        /* softBlockCount++; */
        /* Call scheduler */
        scheduler();
    } else {
        /* Do not block, return */
        loadProcessState(&currentProcess->p_s, 0);
    }
}

/* DONE */
void verhogen(state_PTR exceptionState) { /* SYS4 */
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Get semaphore address from a1 */
    int *semAdd = (int *)currentProcess->p_s.s_a1;

    /* Increment semaphore address */
    (*semAdd)++;

    /* If semaphore is non-positive, unblock process */
    if (*semAdd <= 0) {
        /* Unblock process */
        pcb_PTR p = removeBlocked(semAdd);
        if (p != NULL) {
            insertProcQ(&readyQueue, p);    /* Insert unblocked process into ready queue */
            /* softBlockCount--; */
        }
    }

    /* Load process state with current process state */
    loadProcessState(&currentProcess->p_s, 0);
}

void waitIO(state_PTR exceptionState) { /* SYS5 */
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Get line and device number from a1 and a2 */
    int line = currentProcess->p_s.s_a1;
    int device = currentProcess->p_s.s_a2;
    int term_read = currentProcess->p_s.s_a3;  /* For terminals: 1 for read, 0 for write */
    
    /* Calculate device semaphore index */
    int dev_sem_index;
    if (line == TERMINT && term_read)
        dev_sem_index = DEVPERINT * (line - 3) + device + DEVPERINT;
    else
        dev_sem_index = DEVPERINT * (line - 3) + device;
    
    /* Perform V operation on device semaphore */
    deviceSemaphores[dev_sem_index]++;
    
    loadProcessState(&currentProcess->p_s, 0);
}

/* DONE */
void getCpuTime(state_PTR exceptionState) { /* SYS6 */
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */
    
    /* Store cpuTime in s_v0 */
    currentProcess->p_s.s_v0 = currentProcess->p_time;
    
    /* Return cpuTime in v0 */
    loadProcessState(&currentProcess->p_s, 0);
}

/* DONE */
void waitClock(state_PTR exceptionState) { /* SYS7 */
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Get pseudo-clock semaphore index */
    int psem = DEVICE_COUNT; /* Pseudo-clock semaphore index */
    
    /* Perform P operation */
    deviceSemaphores[psem]--;
    
    /* If negative, block the process (Should always happen) */
    if (deviceSemaphores[psem] < 0) {
        /* Block the process */
        softBlockCount++;
        insertBlocked(&deviceSemaphores[psem], currentProcess);
        scheduler();
    }
    
    /* Load process state with current process state */
    loadProcessState(exceptionState, 0);
}

/* DONE */
void getSupportPtr(state_PTR exceptionState) { /* SYS8 */
    /* Already incremented PC in syscallHandler */
    /* Current Process already updated with CPU time, new process state (exceptionState) in syscallHandler */

    /* Return support structure pointer in v0 */
    currentProcess->p_s.s_v0 = (int)currentProcess->p_supportStruct;
    
    /* Load process state with current process state's support structure */
    loadProcessState(&currentProcess->p_s, 0);
}

/* DONE */
/* function to perform deep copy of state */
void copyState(state_t *dest, state_t *src) {
    if (dest == NULL || src == NULL) {
        return;
    }

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

/* DONE */
void uTLB_RefillHandler() {
    /* *
    * Provided skeleton TLB-Refill event handler.
    * Writers of the Support Level (Level 4/Phase 3) will replace/overwrite
    * the contents of this function with their own code/implementation [3].
    */
    setENTRYHI(KUSEG);
    setENTRYLO(KSEG0);
    TLBWR();
    LDST((state_PTR)BIOSDATAPAGE);
}
 
/* DONE */
void passUpOrDie(int exceptionType, state_PTR exceptionState) {
    /* *
    * Implements the "Pass Up or Die" logic [1, 9, 10, 13, 14]. If the
    * current process has a Support Structure, the exception is passed up to
    * the Support Level. Otherwise, the process is terminated [14].
    */
    /* Check if current process has a support structure */
    if (currentProcess != NULL && currentProcess->p_supportStruct != NULL) {
        /* *Pass up to the Support Level */

        /* *Copy the saved exception state from the BIOS Data Page to the
        * correct sup_exceptState field of the Current Process [23].
        */
        copyState(&currentProcess->p_supportStruct->sup_exceptState[exceptionType], exceptionState);

        /* *Perform a LDCXT using the fields from the correct
        * sup_exceptContext field of the Current Process [23].
        */
        LDCXT(
            currentProcess->p_supportStruct->sup_exceptContext[exceptionType].c_stackPtr,
            currentProcess->p_supportStruct->sup_exceptContext[exceptionType].c_status,
            currentProcess->p_supportStruct->sup_exceptContext[exceptionType].c_pc);
    } else {
        /* *No support structure, terminate the process [14, 29, 30] */
        terminateProcess(currentProcess);
        /* Call scheduler */
        scheduler();
    }
}

/* DONE */
void tlbExceptionHandler(state_PTR exceptionState) {
    /* *
    * Handles TLB exceptions [1, 13]. Determines the cause of the TLB
    * exception and either handles it or passes it up to the Support Level [14].
    */
    passUpOrDie(PGFAULTEXCEPT, exceptionState); /* PassUpOrDie [9, 10] */
}

/* DONE */
void programTrapHandler(state_PTR exceptionState) {
    /* *
    * Handles Program Trap exceptions [1, 13]. Determines the cause of the
    * Program Trap exception and either handles it or passes it up to the
    * Support Level [14].
    */
    /* Implementation of program trap exception handling logic here */
    passUpOrDie(GENERALEXCEPT, exceptionState); /* PassUpOrDie [10] */
}
