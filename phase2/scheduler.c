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
/* Helper function to load process state with proper error checking */
void loadProcessState(state_t *state)
{
    if (state == NULL)
    {
        PANIC(); /* **If the provided state is NULL, halt the system**. Prevents the system from attempting to load a non-existent process state, which would lead to unpredictable behavior [3.2, 3.7] */
    }

    state->s_reg[6] = state->s_pc; /* **Ensure t9 is equal to PC**. t9/s_reg must be set to PC/s_pc before LDST.  This is crucial for proper execution because the processor requires t9 to mirror the PC [3.4, 7.4].*/

    /* Clear KUc and VMp bits, set KUp to kernel-mode*/
    state->s_status &= ~(STATUS_VMp | STATUS_KUc); /* Clear VM (Virtual Memory) and KUc (Kernel/User current mode) bits.  This ensures the processor starts in kernel mode with virtual memory disabled [3.5.10, 7.4].*/
    state->s_status |= STATUS_KUp;                 /* Set KUp (Kernel/User previous mode) bit for kernel mode after LDST. The KUp value will become KUc after LDST.  This prepares the Status register for the transition to kernel mode [3.5.10, 7.4].*/

    /* Set IEp to enable interrupts*/
    state->s_status |= STATUS_IEp; /* Set IEp (Interrupt Enable previous mode) bit to enable interrupts after LDST. The IEp value will become IEc after LDST.  This prepares the Status register to enable interrupts [3.5.10, 7.4].*/

    state->s_status |= (STATUS_TE | IMON); /* Enable Timer and Interrupt Mask. This sets the Timer Enable (TE) and Interrupt Mask (IM) bits in the status register to enable timer interrupts [3.6, 7.4].*/

    /* Set PLT*/
    setTIMER(QUANTUM); /* Load the Processor Local Timer (PLT) with the QUANTUM value. This determines the time slice for the process, implementing preemptive scheduling [3.2, 3.6].*/

    /* Load processor state*/
    LDST(state); /* Load the processor state using the LDST (Load Processor State) instruction. This atomically loads all relevant registers from the provided state structure, starting the process execution [3.2, 7.3.1, 7.4].*/
}

/*DONE */
void scheduler()
{
    /* Get next process from ready queue */
    currentProcess = removeProcQ(&readyQueue); /* Removes the next process from the ready queue for execution [3.2, 26].*/

    if (currentProcess != NULL)
    {
        /* Update system start time for next process */
        STCK(startTOD); /* Captures the current time of day (TOD) before the process starts executing. This is used for CPU time accounting [3.8, 56].*/

        /* Initialize new process execution */
        if (currentProcess->p_time == 0)
        {
            /* First time running this process */
            currentProcess->p_s.s_status |= STATUS_TE;   /* Enable timer. Enables the Processor Local Timer (PLT) for this process, ensuring it gets a time slice [3.6.3, 51].*/
            currentProcess->p_s.s_status &= ~STATUS_VMp; /* Disable VM.  Disables virtual memory for this process, assuming physical addressing is used in this phase [3.3, 26].*/
        }

        /* Load process state and start execution */
        loadProcessState(&currentProcess->p_s); /* Loads the process state using the loadProcessState function, which sets up the processor and starts the process [3.2, 36].*/
    }
    /* If no ready processes but there are blocked processes */
    else if (processCount > 0)
    {
        if (softBlockCount > 0)
        {
            /* Set timer and wait for interrupt */
            setTIMER(CLOCKINTERVAL); /* Sets the timer with a clock interval for handling blocked processes [3.6.3, 53].*/

            /* Enable all interrupts and wait */
            unsigned int status = getSTATUS();        /* Get current status [3].*/
            status |= (STATUS_IE | STATUS_TE | IMON); /* Enable interrupts, timer, and interrupt mask [3.6.3, 53].*/
            status &= ~(STATUS_KUc | STATUS_VMp);     /* Ensure kernel mode and disable VM [3.6.3, 53].*/
            setSTATUS(status);                        /* Set the status register with the modified value [3.6.3, 53, 117].*/
            WAIT();                                   /* Put the processor in a wait state until an interrupt occurs, unblocking a process [3.2, 37, 7.2.2].*/
        }
        else
        {
            /* Deadlock: processes exist but none are ready or blocked */
            PANIC(); /* Indicates a deadlock situation where processes exist but are neither ready nor blocked, so the system cannot proceed [3.2, 37, 7.3.6].*/
        }
    }
    else
    {
        /* System finished - no more processes */
        HALT(); /* Halts the system when there are no more processes to run, indicating the completion of all tasks [3.2, 37, 7.3.7].*/
    }
}
