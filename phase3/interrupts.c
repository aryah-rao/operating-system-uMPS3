/******************************* interrupts.c *************************************
 *
 * Module: Interrupt Handling
 *
 * Description:
 * This module handles all hardware interrupts including device I/O, processor
 * local timer (PLT), and interval timer interrupts. It detects interrupt sources,
 * acknowledges interrupts, and manages process wakeups for I/O completion.
 *
 * Implementation:
 * The module uses a priority-based approach to handle interrupts, checking
 * the cause register to determine which interrupt occurred. It contains specific
 * handlers for timer-related interrupts and device I/O interrupts. For device
 * interrupts, it identifies the specific device, acknowledges the interrupt,
 * and unblocks any process waiting for the device. For timer interrupts, it
 * handles PLT (processor local timer) interrupts, which occur when a process's
 * time quantum expires, and IT (interval timer) interrupts, which initiates a
 * system clock tick every 100ms.
 * 
 * Time Policy:
 * The module updates the current process's CPU time, stores quantum left and 
 * stores the latest process state from the BIOS data page before handling timer
 * interrupts. This ensures that the time spent in the nucleus is FREE. The current
 * process is then loaded with the remaining quantum.
 * 
 * Functions:
 * - interruptHandler: Main interrupt handler that routes interrupts to appropriate
 *                      handlers.
 * - handlePLT: Handles processor local timer interrupts.
 * - handlePseudoClock: Handles interval timer interrupts.
 * - handleNonTimerInterrupt: Handles device I/O interrupts.
 * - getDeviceNumber: Identifies the specific device that generated an interrupt.
 * - acknowledge: Acknowledges an interrupt for a specific device.
 *
 * Written by Aryah Rao and Anish Reddy
 *
 ***************************************************************************/

/******************** Included Header Files ********************/
#include "../h/interrupts.h"

/******************** Function Prototypes ********************/
HIDDEN void handlePseudoClock();
HIDDEN void handlePLT();
HIDDEN void handleNonTimerInterrupt(int line);
HIDDEN int getDeviceNumber(unsigned int devMap);
HIDDEN unsigned int acknowledge(int line, int devNum, devregarea_t* devRegisterArea, int devSemaphore);

/******************** Function Definitions ********************/

/* ========================================================================
 * Function: interruptHandler
 *
 * Description: Main interrupt handler that identifies the interrupt source
 *              and dispatches to appropriate specific handlers.
 * 
 * Parameters:
 *              None (accesses exception state from BIOS data page)
 * 
 * Returns:
 *              Does not return (transfers control to specific handler or scheduler)
 * ======================================================================== */
void interruptHandler() {
    /* Get old processor state from BIOS data page */
    state_t* interruptState = (state_t*) BIOSDATAPAGE;

    /* Update current process state and get remaining quantum */
    int quantumLeft = updateCurrentProcess(interruptState);

    /* Get the interrupt cause bits from cause register */
    int cause = interruptState->s_cause;

    /* Process the interrupt based on its type - checking interrupt pending bits */
    if (cause & PLTINTERRUPT) {
        /* Processor Local Timer interrupt (quantum expired) */
        handlePLT();
    } else if (cause & ITINTERRUPT) {
        /* Interval Timer interrupt (pseudoclock) */
        handlePseudoClock();
    } else if (cause & DISKINTERRUPT) {
        /* Disk device interrupt */
        handleNonTimerInterrupt(DISKINT);
    } else if (cause & FLASHINTERRUPT) {
        /* Flash device interrupt */
        handleNonTimerInterrupt(FLASHINT);
    } else if (cause & NETWORKINTERRUPT) {
        /* Network device interrupt */
        handleNonTimerInterrupt(NETWINT);
    } else if (cause & PRINTERINTERRUPT) {
        /* Printer device interrupt */
        handleNonTimerInterrupt(PRNTINT);
    } else if (cause & TERMINTERRUPT) {
        /* Terminal device interrupt */
        handleNonTimerInterrupt(TERMINT);
    } else {
        /* Unknown interrupt type - critical error */
        PANIC();
    }

    /* Resume execution of current process or call scheduler */
    if (currentProcess != mkEmptyProcQ()) {
        loadProcessState(&currentProcess->p_s, quantumLeft);
    } else {
        /* No current process - call scheduler to select next process */
        scheduler();
    }
}

/* ========================================================================
 * Function: handlePLT
 *
 * Description: Handles Processor Local Timer (PLT) interrupts which occur
 *              when a process's time quantum expires.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              None (control returns to interruptHandler)
 * ======================================================================== */
HIDDEN void handlePLT() {
    /* Current process state and CPU time have been updated in interruptHandler */

    /* Acknowledge the interrupt by resetting the interval timer */
    setTIMER(CLOCKINTERVAL);

    if (currentProcess != mkEmptyProcQ()) {
        /* Move process from high to low priority queue when quantum expires */
        insertProcQ(&readyQueueLow, currentProcess);

        /* Reset current process pointer - a new process will be selected */
        currentProcess = mkEmptyProcQ();
    }

    /* Returns to interruptHandler which will either resume process or call scheduler */
}

/* ========================================================================
 * Function: handlePseudoClock
 *
 * Description: Handles Interval Timer interrupts which provide the system
 *              clock tick for time-delayed operations.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              None (control returns to interruptHandler)
 * ======================================================================== */
HIDDEN void handlePseudoClock() {
    /* Current process state and CPU time have been updated in interruptHandler */

    /* Acknowledge the interrupt by reloading the interval timer */
    LDIT(CLOCKINTERVAL);
    
    /* Wake up all processes blocked on pseudoclock semaphore */
    pcb_PTR p;
    while ((p = removeBlocked(&deviceSemaphores[DEVICE_COUNT-1])) != mkEmptyProcQ()) {
        /* Decrement soft block count and add process to high priority ready queue */
        softBlockCount--;
        insertProcQ(&readyQueueHigh, p);
    }
    
    /* Reset pseudoclock semaphore to initial state */
    deviceSemaphores[DEVICE_COUNT-1] = 0;

    /* Returns to interruptHandler which will either resume process or call scheduler */
}

/* ========================================================================
 * Function: handleDeviceInterrupt
 *
 * Description: Handles interrupts from I/O devices by identifying the specific
 *              device, acknowledging the interrupt, and unblocking any waiting
 *              process.
 * 
 * Parameters:
 *              line - Interrupt line number (3-7)
 * 
 * Returns:
 *              None (control returns to interruptHandler)
 * ======================================================================== */
HIDDEN void handleNonTimerInterrupt(int line) {
    /* Access device registers from RAMBASE address */
    devregarea_t* devRegisterArea = (devregarea_t*) RAMBASEADDR;
    
    /* Get the interrupt device map for this line */
    unsigned int devMap = devRegisterArea->interrupt_dev[line - MAPINT];
    
    /* Identify which specific device triggered the interrupt */
    int devNum = getDeviceNumber(devMap);
    
    /* Calculate device semaphore index based on line and device number */
    int devSemaphore = ((line - MAPINT) * DEV_PER_LINE) + devNum;
    
    /* Acknowledge the interrupt and get device status code */
    unsigned int status = acknowledge(line, devNum, devRegisterArea, devSemaphore);
    
    /* Perform V operation to unblock any process waiting on this device */
    pcb_PTR unblockedProcess = verhogen(&deviceSemaphores[devSemaphore]);

    /* If a process was unblocked, pass the device status to it */
    if (unblockedProcess != mkEmptyProcQ()) {
        unblockedProcess->p_s.s_v0 = status;
        softBlockCount--;
    }
    
    /* Returns to interruptHandler which will either resume process or call scheduler */
}

/* ========================================================================
 * Function: getDeviceNumber
 *
 * Description: Identifies the specific device number that generated an interrupt
 *              by analyzing the interrupt pending bits.
 * 
 * Parameters:
 *              devMap - Bitmask of pending device interrupts
 * 
 * Returns:
 *              Device number (0-7) or ERROR if no device found
 * ======================================================================== */
HIDDEN int getDeviceNumber(unsigned int devMap) {
    /* Check each bit position to identify which device triggered the interrupt */
    if (devMap & (PROCINTERRUPT >> 8)) {
        return PROCINT;          /* Processor interrupt (0) */
    } else if (devMap & (PLTINTERRUPT >> 8)) {
        return PLTINT;           /* Processor local timer (1) */
    } else if (devMap & (ITINTERRUPT >> 8)) {
        return ITINT;            /* Interval timer (2) */
    } else if (devMap & (DISKINTERRUPT >> 8)) {
        return DISKINT;          /* Disk device (3) */
    } else if (devMap & (FLASHINTERRUPT >> 8)) {
        return FLASHINT;         /* Flash device (4) */
    } else if (devMap & (NETWORKINTERRUPT >> 8)) {
        return NETWINT;          /* Network device (5) */
    } else if (devMap & (PRINTERINTERRUPT >> 8)) {
        return PRNTINT;          /* Printer device (6) */
    } else if (devMap & (TERMINTERRUPT >> 8)) {
        return TERMINT;          /* Terminal device (7) */
    } else {
        /* No matching device found - critical error */
        PANIC();
        return ERROR;            /* -1, should never reach here */
    }
}

/* ========================================================================
 * Function: acknowledge
 *
 * Description: Acknowledges an interrupt for a specific device and returns
 *              its status code.
 * 
 * Parameters:
 *              line - Interrupt line number
 *              devNum - Device number within the line
 *              devRegisterArea - Pointer to device register area
 *              devSemaphore - Index to device semaphore
 * 
 * Returns:
 *              Device status code after acknowledgment
 * ======================================================================== */
HIDDEN unsigned int acknowledge(int line, int devNum, devregarea_t* devRegisterArea, int devSemaphore) {
    unsigned int status;

    /* Special handling for terminal devices */
    if (line == TERMINT) {
        /* Check if it's a transmit or receive interrupt by examining status bit */
        if (devRegisterArea->devreg[devSemaphore].t_transm_status & TRANSM_BIT) {
            /* Transmit interrupt (write operation) */
            status = devRegisterArea->devreg[devSemaphore].t_transm_status;
            /* Acknowledge the interrupt by writing ACK to the command register */
            devRegisterArea->devreg[devSemaphore].t_transm_command = ACK;
        } else {
            /* Receive interrupt (read operation) */
            status = devRegisterArea->devreg[devSemaphore].t_recv_status;
            /* Acknowledge the interrupt by writing ACK to the command register */
            devRegisterArea->devreg[devSemaphore].t_recv_command = ACK;
        }
    } else {
        /* Standard handling for non-terminal devices */
        status = devRegisterArea->devreg[devSemaphore].d_status;
        /* Acknowledge the interrupt by writing ACK to the command register */
        devRegisterArea->devreg[devSemaphore].d_command = ACK;
    }
    
    return status;
}
