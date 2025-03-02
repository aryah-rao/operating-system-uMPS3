/**************************** interrupts.c **********************************
*
* The interrupt handling module for the PandOS Operating System.
* Handles all device interrupts and timer interrupts.
*
* Functions:
* - interruptHandler: Main interrupt dispatcher
* - handlePseudoClock: Interval timer handler
*
*********************************************************************/

#include "../h/interrupts.h"

/* Forward declarations for helper functions */
HIDDEN int getHighestPriorityInt(unsigned int cause);
HIDDEN void handlePseudoClock();
HIDDEN void handlePLT();
HIDDEN void handleDeviceInterrupt(int lineNum);
HIDDEN int getDeviceNumber(unsigned int deviceMap);
HIDDEN unsigned int acknowledge(int lineNum, int devNum, devregarea_t* devRegs, int devIndex);

/* Current TOD */
HIDDEN cpu_t currentTime;
HIDDEN unsigned int quantumLeft;

/* Main interrupt handler - Called when an interrupt occurs */
void interruptHandler() {
    /* Get old processor state from BIOS area (0x0FFFF000) as specified in PandOS spec */
    state_t* interruptState = (state_t*) BIOSDATAPAGE;

    /* Save the current process's CPU time by calculating elapsed time since process started */
    if (currentProcess != NULL) {
        /* Deep copy state from BIOS data page to current process state */
        copyState(&currentProcess->p_s, interruptState);
        
        /* Update CPU time */ 
        STCK(currentTime);  /* Read current time using STCK macro */
        currentProcess->p_time += (currentTime - startTOD); /* Add elapsed time to process's accumulated CPU time */
        startTOD = currentTime; /* Update startTOD for next calculation */

        /* Calculate quantum left before interrupt occurred */
        quantumLeft = getTIMER();
    }

    /* Get the interrupt cause, which is the interrupt that triggered this handler */
    unsigned int cause = (interruptState->s_cause & CAUSE_IP_MASK) >> CAUSE_IP_SHIFT; /* Interrupt Pending bits */
    
    /* Get the highest priority interrupt */
    int line = getHighestPriorityInt(cause);
    
    /* Process the interrupt based on its type */
    switch (line) {
        case PLT_INT:
            /* Processor Local Timer interrupt (quantum expired) */
            handlePLT();
            break;
            
        case INTERVAL_INT:
            /* Interval Timer interrupt (for pseudo-clock) */
            handlePseudoClock();
            break;
            
        case DISK_INT:
        case FLASH_INT:
        case NETWORK_INT:
        case PRINTER_INT:
        case TERMINAL_INT:
            /* All device interrupts are handled by handleDeviceInterrupt */
            handleDeviceInterrupt(line);
            break;
            
        default:
            /* Unknown interrupt type - system error */
            PANIC();
    }

    /* Resume execution of current process or call scheduler */
    if (currentProcess != NULL) {
        loadProcessState(&currentProcess->p_s, quantumLeft);
    } else {
        /* No current process - call scheduler to select next process */
        scheduler();
    }
}

/* Determine highest priority interrupt from the cause register bits */
HIDDEN int getHighestPriorityInt(unsigned int cause) {
    int i;
    /* Scan through all interrupt lines from highest (7) to lowest (0) */
    for (i = 7; i >= 0; i--) {
        if (cause & CAUSE_IP(i)) 
            return i;  /* Return the highest priority interrupt found */
    }
    return -1;  /* No interrupts pending - should not happen in this context */
}

/* DONE */
/* PLT interrupt handler: Qunatum Expired*/
HIDDEN void handlePLT() {
    /* Already updated with CPU time, new process state in interruptlHandler */

    /* Acknowledge interrupt */
    setTIMER(CLOCKINTERVAL);

    if (currentProcess != NULL) {
        /* Place process back in ready queue */
        insertProcQ(&readyQueue, currentProcess);

        /* Reset current process */
        currentProcess = NULL;
    }

    /* Goes back to interruptHandler where it loads the current process state or calls scheduler */
}

/* DONE */
/* Pseudo-clock timer handler */
HIDDEN void handlePseudoClock() {
    /* Already updated with CPU time, new process state in interruptlHandler */

    /* Acknowledge interrupt by loading new interval */ /* IDKKKKK */
    LDIT(CLOCKINTERVAL);
    
    /* Wake up all processes blocked on pseudo-clock */
    pcb_PTR p;
    while ((p = removeBlocked(&deviceSemaphores[DEVICE_COUNT])) != mkEmptyProcQ()) {
        softBlockCount--;
        insertProcQ(&readyQueue, p);
    }
    
    /* Reset pseudo-clock semaphore */
    deviceSemaphores[DEVICE_COUNT] = 0;

    /* Goes back to interruptHandler where it loads the current process state or calls scheduler */
}



/*
 * Extracts the device number from an interrupt bitmask.
 * 
 * Parameters:
 *   deviceMap - Bitmask representing the active device interrupt
 * 
 * Returns: The device number (0-7) or -1 if not found
 */
HIDDEN int getDeviceNumber(unsigned int deviceMap) {
	if(deviceMap & 0x00000001) {
		return PROCESSOR_INT; }
	else if(deviceMap & 0x00000002) {
		return PLT_INT; }
	else if(deviceMap & 0x00000004) {
		return INTERVAL_INT; }
	else if(deviceMap & 0x00000008) {
		return DISK_INT; }
	else if(deviceMap & 0x00000010) {
		return FLASH_INT; }
	else if(deviceMap & 0x00000020) {
		return NETWORK_INT; }
	else if(deviceMap & 0x00000040) {
		return PRINTER_INT; }
	else if(deviceMap & 0x00000080) {
		return TERMINAL_INT; }
    else {
        /* No device found */
        PANIC();
        return -1;
    }
}

/*
 * Acknowledges an interrupt for a given device and returns its status.
 * 
 * Parameters:
 *   lineNum - The interrupt line number
 *   devNum - The device number
 * 
 * Returns: The status code of the device after acknowledgment
 */
HIDDEN unsigned int acknowledge(int lineNum, int devNum, devregarea_t* devRegs, int devIndex) {

    unsigned int status;

    if (lineNum == TERMINT) { /* Terminal device special case */
        /* For terminals, check if it's a transmit or receive interrupt */
        if ((devRegs->devreg[devIndex].t_transm_status & DEV_TERM_READY) != DEV_TERM_READY) {
            /* Transmit interrupt */
            status = devRegs->devreg[devIndex].t_transm_status;
            devRegs->devreg[devIndex].t_transm_command = ACK;
            /* Don't adjust devIndex here, we'll do it in the handler */
        } else {
            /* Receive interrupt */
            status = devRegs->devreg[devIndex].t_recv_status;
            devRegs->devreg[devIndex].t_recv_command = ACK;
            devIndex += DEVPERINT; /* Adjust index for receive interrupts */
        }
    } else { /* General case for non-terminal devices */
        status = devRegs->devreg[devIndex].d_status;
        devRegs->devreg[devIndex].d_command = ACK;
    }

    return status;
}

/*
 * Handle device interrupts from any non-timer I/O device.
 * This is the main entry point called from interruptHandler.
 * 
 * Parameters:
 *   lineNum - The interrupt line that was triggered (3-7)
 */
HIDDEN void handleDeviceInterrupt(int lineNum) {
    /* Get interrupt device map to identify which device triggered the interrupt */
    devregarea_t* devRegs = (devregarea_t*) RAMBASEADDR;
    unsigned int deviceMap = devRegs->interrupt_dev[lineNum - DISKINT];
    
    /* Find which device triggered the interrupt */
    int devNum = getDeviceNumber(deviceMap);
    if (devNum == -1) {
        /* No valid device found - shouldn't happen */
        PANIC();
        return;
    }
    
    /* Calculate device semaphore index */
    int devIndex = ((lineNum - DISKINT) * DEVPERINT) + devNum;
    
    /* Acknowledge interrupt and get status code */
    unsigned int status = acknowledge(lineNum, devNum, devRegs, devIndex);
    
    /* For terminals, we might need to adjust the semaphore index for receive operations */
    if (lineNum == TERMINT) {
        /* If it's a receive interrupt, adjust the semaphore index */
        if ((status & DEV_TERM_STATUS) == DEV_TERM_READY) {
            devIndex += DEVPERINT; /* Point to terminal receive semaphore */
        }
    }
    
    /* Perform V operation on semaphore to unblock any waiting process */
    pcb_PTR unblockedProcess = verhogen(devIndex);

    /* Store status code in v0 register */
    if (unblockedProcess != NULL) {
        unblockedProcess->p_s.s_v0 = status;
    }
    
    /* Control is returned to interruptHandler, which will either
     * resume the current process or call the scheduler as needed */
}
