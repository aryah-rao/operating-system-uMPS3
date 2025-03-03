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
HIDDEN void handlePseudoClock();
HIDDEN void handlePLT();
HIDDEN void handleDeviceInterrupt(int lineNum);
HIDDEN int getDeviceNumber(unsigned int deviceMap);
HIDDEN unsigned int acknowledge(int lineNum, int devNum, devregarea_t* devRegs, int devIndex);


/* Main interrupt handler - Called when an interrupt occurs */
void interruptHandler() {
    /* Get old processor state from BIOS area (0x0FFFF000) */
    state_t* interruptState = (state_t*) BIOSDATAPAGE;

    /* Update current process state */
    int quantumLeft = updateCurrentProcess(interruptState);

    /* Get the interrupt cause */
    int cause = interruptState->s_cause;

    /* Process the interrupt based on its type */
    if (cause & PLTINTERRUPT) {
        /* Processor Local Timer interrupt (quantum expired) */
        handlePLT();
    } else if (cause & ITINTERRUPT) {
        /* Interval Timer interrupt (for pseudo-clock) */
        handlePseudoClock();
    } else if (cause & DISKINTERRUPT) {
        /* Disk interrupt */
        handleDeviceInterrupt(DISK);
    } else if (cause & FLASHINTERRUPT) {
        /* Flash interrupt */
        handleDeviceInterrupt(FLASH);
    } else if (cause & NETWORKINTERRUPT) {
        /* Network interrupt */
        handleDeviceInterrupt(NETWORK);
    } else if (cause & PRINTERINTERRUPT) {
        /* Printer interrupt */
        handleDeviceInterrupt(PRINTER);
    } else if (cause & TERMINTERRUPT) {
        /* Terminal interrupt */
        handleDeviceInterrupt(TERMINAL);
    } else {
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
        if (devRegs->devreg[devIndex].t_transm_status & TRANSM_BIT) {
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
    
    /* Perform V operation on semaphore to unblock any waiting process */
    pcb_PTR unblockedProcess = verhogen(&deviceSemaphores[devIndex]);

    /* Store status code in v0 register */
    if (unblockedProcess != NULL) {
        unblockedProcess->p_s.s_v0 = status;
    }
    
    /* Control is returned to interruptHandler, which will either
     * resume the current process or call the scheduler as needed */
}
