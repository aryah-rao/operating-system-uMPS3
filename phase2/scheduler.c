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

/* DONE */
/* BAAAAAAAAAAAAAM, Context Switch!! */
void loadProcessState(state_t *state, unsigned int quantum) {
    if (state == NULL) {
        PANIC(); /* **If the provided state is NULL, halt the system**. Prevents the system from attempting to load a non-existent process state, which would lead to unpredictable behavior [3.2, 3.7] */
    }

    /* Set PLT */
    if (quantum > 0) {
        setTIMER(quantum); /* Set the timer to the provided quantum. */
    } else {
        setTIMER(QUANTUM); /* Set the timer to the default quantum. */
    }

    /* Update system start time for next process */
    STCK(startTOD);

    /* Load processor state*/
    LDST(state); /* Load the processor state using the LDST (Load Processor State) instruction. This atomically loads all relevant registers from the provided state structure, starting the process execution [3.2, 7.3.1, 7.4].*/
}
 
 /*DONE */
void scheduler() {
    /* Get next process from ready queue */
    currentProcess = removeProcQ(&readyQueue); /* Removes the next process from the ready queue for execution [3.2, 26].*/

    if (currentProcess != NULL) {
        /* Update system start time for next process */
        STCK(startTOD); /* Captures the current time of day (TOD) before the process starts executing. This is used for CPU time accounting [3.8, 56].*/

        /* Load process state and start execution */
        loadProcessState(&currentProcess->p_s, 0); /* Loads the process state using the loadProcessState function, which sets up the processor and starts the process [3.2, 36].*/
    }

    /* If no ready processes but there are blocked processes */
    else if (processCount > 0) {
        if (softBlockCount > 0) {
            /* Set timer and wait for interrupt */
            setTIMER(MAXINT); /* Sets the timer with the maximum possible value so no PLT Interrupts will occur */

            /* Enable all interrupts and wait */
            unsigned int status = getSTATUS();        /* Get current status [3].*/
            status |= (STATUS_IE | STATUS_TE | IMON); /* Enable interrupts, timer, and interrupt mask [3.6.3, 53].*/
            status &= ~(STATUS_KUc | STATUS_VMp);     /* Ensure kernel mode and disable VM [3.6.3, 53].*/
            setSTATUS(status);                        /* Set the status register with the modified value [3.6.3, 53, 117].*/

            /* Wait for interrupt */
            WAIT();
        }
        else {
            /* Deadlock: processes exist but none are ready or blocked */
            PANIC();
        }
    }
    else {
        /* System finished - no more processes */
        HALT();
    }
}
