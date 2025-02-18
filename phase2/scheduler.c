/* scheduler.c */

/******************************************************************
 * Module Level: Implements the Scheduler and the deadlock detector.
 ******************************************************************/

#include "../h/scheduler.h"
#include "../h/initial.h"

HIDDEN int TIME_QUANTUM_PER_QUEUE[NUM_QUEUES];      /* Time quantum values for different priority levels */
HIDDEN pcb_PTR readyQueues[NUM_QUEUES];             /* Ready queues for different priority levels */

/* Function to initialize time quantum values */
HIDDEN void initTimeQuantum() {
    int i;
    for (i = 0; i < NUM_QUEUES; i++) {
      TIME_QUANTUM_PER_QUEUE[i] = TIME_QUANTUM >> i;  /* Equivalent to (TIME_QUANTUM / 2^i) */
    }
}

/* Function to find the highest priority non-empty queue */
HIDDEN int findHighestPriorityQueue() {
    int i;
    for (i = 0; i < NUM_QUEUES; i++) {
        if (!emptyProcQ(readyQueues[i])) {
            return i;
        }
    }
    return -1;
}


/* Called when process's time quantum expires (demotion logic) */
HIDDEN void demoteProcess() {
if (currentProcess != NULL) {
    reinsertProcess(currentProcess, 1); /* Used full quantum, so demote */
    currentProcess = NULL;
}
scheduler();
}

/* Called when a process voluntarily yields before quantum expires (promotion logic) */
HIDDEN void voluntaryYield() {
if (currentProcess != NULL) {
    reinsertProcess(currentProcess, 0); /* Used less than full quantum, check promotion */
    currentProcess = NULL;
}
scheduler();
}

/* Function to reinsert a process into the appropriate queue after demotion or promotion */
HIDDEN void reinsertProcess(pcb_t *process, int usedFullQuantum) {
    /* If the process used its full quantum, demote it (if possible) */
    if (usedFullQuantum && process->priority < NUM_QUEUES - 1) {
        process->priority++;  /* Move to a lower priority queue */
    } 
    /* If the process did not use its quantum twice, promote it */
    else if (!usedFullQuantum) {
        process->earlyExits++;
        if (process->earlyExits >= 2 && process->priority > 0) {
            process->priority--;  /* Move to a higher priority queue */
            process->earlyExits = 0; /* Reset early exit count */
        }
    } else {
        process->earlyExits = 0;  /* Reset early exit count on demotion */
    }

    insertProcQ(&readyQueues[process->priority], process);
}

/* Main MLFQ Scheduler function */
void scheduler() {
    int queueIdx = findHighestPriorityQueue();

    /* Remove a pcb from the Ready Queue */
    if (queueIdx == -1) {
        /* Ready Queue is empty */
        if (processCount == 0) {
            /* No more processes to run */
            HALT(); /* HALT execution */
        } else if (softBlockCount > 0) {
            /* Wait State */
            /* Set correct status before entering WAIT state */

            WAIT(); /* WAIT for an I/O to complete */
        } else {
            PANIC(); /* Deadlock detected */
        }
    } else {
        /* Select the next process from the highest-priority non-empty queue */
        currentProcess = removeProcQ(&readyQueues[queueIdx]);

        /* Set the time slice for this priority level using PLT */
        setTIMER(TIME_QUANTUM_PER_QUEUE[currentProcess->priority]);

        /* Load the process's processor state */
        LDST(&currentProcess->p_s);
    }
}
