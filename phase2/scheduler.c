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
    /* Check if state is NULL */
    if (state == NULL) {
        PANIC(); /* Handle error */
    }

    /* Set PLT */
    if (quantum > 0) {
        setTIMER(quantum); /* Set the timer to the provided quantum. */
    } else {
        setTIMER(QUANTUM); /* Set the timer to the default quantum. */
    }

    /* Update system start time */
    STCK(startTOD);

    /* Load processor state*/
    LDST(state);
}
 
 /*DONE */
void scheduler() {
    /* Get next process from ready queue */
    currentProcess = removeProcQ(&readyQueue);

    if (currentProcess != NULL) {
        /* Load process state and start execution */
        loadProcessState(&currentProcess->p_s, 0);
    }

    /* If no ready processes */
    else if (processCount > 0) {
        /* Check for blocked processes */
        if (softBlockCount > 0) {
            /* Set timer and wait for interrupt */
            setTIMER(MAXINT);

            /* Enable all interrupts and wait */
            setSTATUS(ALLOFF | IECON | IMON);

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
