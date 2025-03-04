/******************************* scheduler.c *************************************
 *
 * Module: Scheduler
 *
 * Description:
 * This module implements the process scheduling algorithm for the PandOS
 * operating system. It selects which process to run next, manages the 
 * ready queues, and handles CPU time allocation.
 *
 * Implementation:
 * The module implements a two-level priority scheduler with round-robin 
 * time slicing. High priority processes are always scheduled before low 
 * priority ones. The scheduler also handles deadlock detection and system
 * shutdown when no more processes exist.
 *
 * Written by Aryah Rao and Anish Reddy
 *
 ***************************************************************************/

/******************** Included Header Files ********************/
#include "../h/scheduler.h"

/******************** Function Definitions ********************/

/* ========================================================================
 * Function: loadProcessState
 *
 * Description: Loads a process state into the processor and starts execution.
 *              Sets up the process quantum timer and updates system time.
 * 
 * Parameters:
 *              state - Pointer to the process state to be loaded
 *              quantum - Time quantum value to assign (0 for default QUANTUM)
 * 
 * Returns:
 *              This function does not return (control passes to the loaded process)
 * ======================================================================== */
void loadProcessState(state_t *state, unsigned int quantum) {
    /* Check if state is NULL */
    if (state == NULL) {
        PANIC(); /* Handle error */
    }

    /* Set Processor Local Timer */
    if (quantum > 0) {
        setTIMER(quantum); /* Set the timer to the provided quantum */
    } else {
        setTIMER(QUANTUM); /* Set the timer to the default quantum */
    }

    /* Update system start time */
    STCK(startTOD);

    /* Load processor state and transfer control */
    LDST(state);
}

/* ========================================================================
 * Function: getProcess
 *
 * Description: Gets the next process to run from the ready queues based on 
 *              priority. If a specific process is requested, it attempts to
 *              remove that process.
 * 
 * Parameters:
 *              process - Pointer to a specific process to remove, or NULL to
 *                        get the next process in queue order
 * 
 * Returns:
 *              Pointer to the selected process, or NULL if no process is available
 * ======================================================================== */
pcb_PTR getProcess(pcb_PTR process) {
    /* If process is not NULL, use outProcQ to remove specific process from ready queues */
    if (process != mkEmptyProcQ()) {
        /* First check high priority queue */
        pcb_PTR removedProcess = outProcQ(&readyQueueHigh, process);

        /* If not in high priority queue, check low priority queue */
        if (removedProcess != mkEmptyProcQ()) {
            return removedProcess;
        }
        return outProcQ(&readyQueueLow, process);
    }
    
    /* Process is NULL, use removeProcQ to get the next process in queue order */
    /* First check high priority queue */
    pcb_PTR removedProcess = removeProcQ(&readyQueueHigh);
    
    /* If high priority queue is empty, check low priority queue */
    if (removedProcess != mkEmptyProcQ()) {
        return removedProcess;
    }
    
    return removeProcQ(&readyQueueLow);
}

/* ========================================================================
 * Function: scheduler
 *
 * Description: Main scheduling function. Selects the next process to run based
 *              on priority. Handles idle system conditions and deadlock detection.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              This function does not return (except in case of HALT or PANIC)
 * ======================================================================== */
void scheduler() {
    /* Get next process from ready queue based on priority */
    currentProcess = getProcess(mkEmptyProcQ());

    /* If a process is available, load it and start execution */
    if (currentProcess != mkEmptyProcQ()) {
        /* Load process state and start execution with default quantum */
        loadProcessState(&currentProcess->p_s, 0);
    }
    /* No ready processes */
    else if (processCount > 0) {
        /* Check if there are blocked processes waiting for events */
        if (softBlockCount > 0) {
            /* Processes exist but are blocked - enter wait state */
            /* Set timer to maximum value to prevent timer interrupts during wait */
            setTIMER(MAXINT);

            /* Enable all interrupts and wait for an interrupt to unblock processes */
            setSTATUS(ALLOFF | STATUS_IEc | CAUSE_IP_MASK);

            /* Wait for interrupt */
            WAIT();
        }
        else {
            /* Deadlock detected: processes exist but none are ready or blocked */
            PANIC();
        }
    }
    else {
        /* System shutdown - no processes exist */
        HALT();
    }
}

