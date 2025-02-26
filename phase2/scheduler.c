/**************************** scheduler.c **********************************
*
* The scheduling module for the PandOS Operating System Phase 2.
* Implements a round-robin scheduling algorithm with time slicing.
*
* Functions:
* - scheduler: Main scheduling function
* - loadProcessState: Helper function to load process state
*
*********************************************************************/

#include "../h/scheduler.h"

/* Helper function to load process state with proper error checking */
void loadProcessState(state_t *state) {
    if (state == NULL) {
        PANIC();
    }
    
    /* Set up kernel mode and interrupt flags properly */
    state->s_status &= ~(STATUS_KUc | STATUS_VMp | STATUS_KUp);  /* Clear KU, VM, and KUp bits */
    state->s_status |= (STATUS_TE | STATUS_IE | IMON);  /* Enable translation, interrupts, and interrupt mask */
    
    /* Set PLT */
    setTIMER(QUANTUM);
    
    /* Load processor state */
    LDST(state);
}

void scheduler() {
    /* Update CPU time for current process if exists */
    if (currentProcess != NULL) {
        cpu_t currentTime;
        STCK(currentTime);
        currentProcess->p_time += (currentTime - startTOD);
        
        /* Place process back in ready queue if not blocked */
        if (currentProcess->p_semAdd == NULL) {
            insertProcQ(&readyQueue, currentProcess);
        }
        currentProcess = NULL;
    }

    /* Get next process from ready queue */
    currentProcess = removeProcQ(&readyQueue);
    
    if (currentProcess != NULL) {
        /* Update system start time for next process */
        STCK(startTOD);
        
        /* Initialize new process execution */
        if (currentProcess->p_time == 0) {
            /* First time running this process */
            currentProcess->p_s.s_status |= STATUS_TE;  /* Enable timer */
            currentProcess->p_s.s_status &= ~STATUS_VMp;  /* Disable VM */
        }
        
        /* Load process state and start execution */
        loadProcessState(&currentProcess->p_s);
    }
    /* If no ready processes but there are blocked processes */
    else if (processCount > 0) {
        if (softBlockCount > 0) {
            /* Set timer and wait for interrupt */
            setTIMER(CLOCKINTERVAL);
            /* Enable all interrupts and wait */
            unsigned int status = getSTATUS();
            status |= (STATUS_IE | STATUS_TE | IMON);
            status &= ~(STATUS_KUc | STATUS_VMp);
            setSTATUS(status);
            WAIT();
        } else {
            /* Deadlock: processes exist but none are ready or blocked */
            PANIC();
        }
    }
    else {
        /* System finished - no more processes */
        HALT();
    }
}
