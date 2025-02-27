/* exceptions.c */

/******************************************************************
 * Module Level: Implements the TLB, Program Trap, and SYSCALL
 * exception handlers. It also contains the skeleton TLB-Refill event handler.
 ******************************************************************/

#include "../h/exceptions.h"


/* Hidden Functions Declarations */

void tlbExceptionHandler(state_t *exceptionState);
void programTrapHandler(state_t *exceptionState);
void syscallHandler(state_t *exceptionState);
void interruptHandler();
void uTLB_RefillHandler();
void passUpOrDie(int exceptionType, state_t *exceptionState);
void getCpuTime(state_t *exceptionState);
void waitIO(state_t *exceptionState);
void verhogen(state_t *exceptionState);
void passeren(state_t *exceptionState);
void createProcess(state_t *exceptionState);
void terminateProcess(pcb_PTR process);
void getSupportPtr(state_t *exceptionState);
void waitClock(state_t *exceptionState);


HIDDEN void copySupportStruct(support_t *dest, support_t *src);


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
 
void passUpOrDie(int exceptionType, state_t *exceptionState) {
    /* *
    * Implements the "Pass Up or Die" logic [1, 9, 10, 13, 14]. If the
    * current process has a Support Structure, the exception is passed up to
    * the Support Level. Otherwise, the process is terminated [14].
    */
    if (currentProcess != NULL && currentProcess->p_supportStruct != NULL) {
        /* *Pass up to the Support Level [14, 26-28] */

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
    }
}

void exceptionHandler() {
    /* *
    * This is the general exception handler for the Nucleus [4]. It is called
    * by the BIOS after an exception occurs [5]. It saves the state of the
    *  process and then dispatches to the appropriate handler based on the
    * cause of the exception [6].
    */

    state_t *exceptionState = (state_t *)BIOSDATAPAGE; /* Address: 0x0FFF.F000 [3, 7, 8] */
    unsigned int cause = exceptionState->s_cause; /* reads the cause [3] */
    int excCode = (cause & CAUSE_EXCCODE_MASK) >> CAUSE_EXCCODE_SHIFT; /* gets excCode [3, 7] */
    
    switch (excCode) {
        case INTERRUPTS: /* Interrupts [6] */
            interruptHandler();
            break;
        case TLBMOD: /* TLB-Modification Exception [6, 9] */
        case TLBINVLD: /* TLB Invalid Exception: on a Load instr. or instruction fetch [6, 9] */
        case TLBINVLDL: /* TLB Invalid Exception: on a Store instr. [6, 9] */
            tlbExceptionHandler(exceptionState);
        break;
        case SYSCALL_EXCEPTION: /* Syscall Exception [6] */
            syscallHandler(exceptionState);
        break;
        case ADDRINVLD: /* Address Error Exception: on a Load or instruction fetch [6, 10] */
        case ADDRINVLDS: /* Address Error Exception: on a Store instr. [6, 10] */
        case BUSINVLD: /* Bus Error Exception: on an instruction fetch [6, 10] */
        case BUSINVLDL: /* Bus Error Exception: on a Load/Store data access [6, 10] */
        case BREAKPOINT: /* Breakpoint Exception [6, 10] */
        case RESERVEDINST: /* Reserved Instruction Exception [6, 10] */
        case COPROCUNUSABLE: /* Coprocessor Unusable Exception [6, 10] */
        case ARITHOVERFLOW: /* Arithmetic Overflow Exception [6, 10] */
            programTrapHandler(exceptionState);
            break;
        default:
            PANIC(); /* *Unexpected exception code [11] */
    }
}


void tlbExceptionHandler(state_t *exceptionState) {
    /* *
    * Handles TLB exceptions [1, 13]. Determines the cause of the TLB
    * exception and either handles it or passes it up to the Support Level [14].
    */
    passUpOrDie(PGFAULTEXCEPT, exceptionState); /* PassUpOrDie [9, 10] */
}

void programTrapHandler(state_t *exceptionState) {
    /* *
    * Handles Program Trap exceptions [1, 13]. Determines the cause of the
    * Program Trap exception and either handles it or passes it up to the
    * Support Level [14].
    */
    /* Implementation of program trap exception handling logic here */
    passUpOrDie(GENERALEXCEPT, exceptionState); /* PassUpOrDie [10] */
}


void syscallHandler(state_t *exceptionState) {
    /* Increment PC before handling syscall */
    exceptionState->s_pc += WORDLEN;

    /* User mode check */
    if ((exceptionState->s_status & STATUS_KUp) != 0) {
        /* User mode calling privileged SYSCALL */
        exceptionState->s_cause = (exceptionState->s_cause & ~CAUSE_EXCCODE_MASK) | 
                                 (RESERVEDINST << CAUSE_EXCCODE_SHIFT);
        /* Program Trap Exception */
        programTrapHandler(exceptionState);
        return;
    }

    /* Handle syscalls */
    switch (exceptionState->s_a0) {
        case CREATEPROCESS:
            createProcess(exceptionState);
            break;
        case TERMINATEPROCESS:
            terminateProcess(NULL);  /* NULL means terminate current process */
            break;
        case PASSEREN:
            if (exceptionState->s_a1 == 0) {
                passUpOrDie(GENERALEXCEPT, exceptionState);
                return;
            }
            passeren(exceptionState);
            break;
        case VERHOGEN:
            if (exceptionState->s_a1 == 0) {
                passUpOrDie(GENERALEXCEPT, exceptionState);
                return;
            }
            verhogen(exceptionState);
            break;
        case WAITIO:
            if (exceptionState->s_a1 < 3 || exceptionState->s_a1 > 7 || 
                exceptionState->s_a2 < 0 || exceptionState->s_a2 >= DEVPERINT) {
                passUpOrDie(GENERALEXCEPT, exceptionState);
                return;
            }
            waitIO(exceptionState);
            break;
        case GETCPUTIME:
            getCpuTime(exceptionState);
            break;
        case WAITCLOCK:
            waitClock(exceptionState);
            break;
        case GETSUPPORTPTR:
            getSupportPtr(exceptionState);
            break;
        default:
            passUpOrDie(GENERALEXCEPT, exceptionState);
    }
}

/*********************SYSCALL SERVICE IMPLEMENTATIONS**********************/

void createProcess(state_t *exceptionState) { /* SYS1 [8] */
    pcb_PTR newPCB = allocPcb(); /* Allocate new PCB [17] */
    if (newPCB == NULL) {
        exceptionState->s_v0 = -1; /* Return -1 in v0 [8] */
    } else {
        /* Initialize new PCB [10, 19] */
        copyState(&newPCB->p_s, (state_t *)exceptionState->s_a1);
        copySupportStruct(newPCB->p_supportStruct, (support_t *)exceptionState->s_a2);

        insertProcQ(&readyQueue, newPCB); /* Insert into Ready Queue [10] */
        newPCB->p_prnt = currentProcess; /* Set parent [10] */

        /* Increment process count */
        processCount++; /* Increment Process Count [10] */

        exceptionState->s_v0 = 0; /* Return 0 in v0 [8] */
    }
    /* increment PC [22] */
    exceptionState->s_pc += 4;
    loadProcessState(exceptionState, 0);
}

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

void passeren(state_t *exceptionState) { /* SYS3 [11] */
    int *semAdd = (int *)exceptionState->s_a1;

    (*semAdd)--;
    if (*semAdd < 0) {
        /* Block the process */
        exceptionState->s_pc += 4;
        copyState(&currentProcess->p_s, exceptionState);
        insertBlocked(semAdd, currentProcess);
        scheduler();
    } else {
        /* Do not block, return */
        exceptionState->s_pc += 4;
        loadProcessState(exceptionState, 0);
    }
}

void verhogen(state_t *exceptionState) { /* SYS4 */
    int *semAdd = (int *)exceptionState->s_a1;

    (*semAdd)++;
    if (*semAdd <= 0) {
        pcb_PTR p = removeBlocked(semAdd);
        if (p != NULL) {
            insertProcQ(&readyQueue, p);
        }
    }
    exceptionState->s_pc += 4;
    loadProcessState(exceptionState, 0);
}

void waitIO(state_t *exceptionState) { /* SYS5 [12] */
    /* Get line and device number from a1 and a2 */
    int line = exceptionState->s_a1;
    int device = exceptionState->s_a2;
    int term_read = exceptionState->s_a3;  /* For terminals: 1 for read, 0 for write */
    
    /* Calculate device semaphore index */
    int dev_sem_index;
    if (line == TERMINT && term_read)
        dev_sem_index = DEVPERINT * (line - 3) + device + DEVPERINT;
    else
        dev_sem_index = DEVPERINT * (line - 3) + device;
    
    /* Perform P operation on device semaphore */
    deviceSemaphores[dev_sem_index]--;
    
    if (deviceSemaphores[dev_sem_index] < 0) {
        /* Block the process */
        softBlockCount++;
        copyState(&currentProcess->p_s, exceptionState);
        insertBlocked(&deviceSemaphores[dev_sem_index], currentProcess);
        scheduler();
    }
    
    exceptionState->s_pc += WORDLEN;
    loadProcessState(exceptionState, 0);
}

void getCpuTime(state_t *exceptionState) { /* SYS6 [13] */
    /* Get current time */
    cpu_t current;
    STCK(current);
    
    /* Calculate total CPU time */
    exceptionState->s_v0 = currentProcess->p_time + (current - startTOD);
    
    exceptionState->s_pc += WORDLEN;
    loadProcessState(exceptionState, 0);
}


void waitClock(state_t *exceptionState) { /* SYS7 [14] */
    /* Get pseudo-clock semaphore index */
    int psem = DEVICE_COUNT; /* Pseudo-clock semaphore index */
    
    /* Perform P operation */
    deviceSemaphores[psem]--;
    
    if (deviceSemaphores[psem] < 0) {
        /* Block the process */
        softBlockCount++;
        copyState(&currentProcess->p_s, exceptionState);
        insertBlocked(&deviceSemaphores[psem], currentProcess);
        scheduler();
    }
    
    exceptionState->s_pc += WORDLEN;
    loadProcessState(exceptionState, 0);
}

void getSupportPtr(state_t *exceptionState) { /* SYS8 [15] */
    /* Return support structure pointer in v0 */
    exceptionState->s_v0 = (int)currentProcess->p_supportStruct;
    
    exceptionState->s_pc += WORDLEN;
    loadProcessState(exceptionState, 0);
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

HIDDEN void copySupportStruct(support_t *dest, support_t *src) {
    if (dest == NULL || src == NULL) return;
    
    /* Copy ASID */
    dest->sup_asid = src->sup_asid;
    
    /* Copy exception states */
    int i;
    for (i = 0; i < 2; i++) {
        copyState(&dest->sup_exceptState[i], &src->sup_exceptState[i]);
        
        /* Copy exception contexts */
        dest->sup_exceptContext[i].c_stackPtr = src->sup_exceptContext[i].c_stackPtr;
        dest->sup_exceptContext[i].c_status = src->sup_exceptContext[i].c_status;
        dest->sup_exceptContext[i].c_pc = src->sup_exceptContext[i].c_pc;
    }
}