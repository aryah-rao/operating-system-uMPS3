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
HIDDEN void handleDeviceInterrupt(int line);
HIDDEN int getDeviceNumber(unsigned int devMap);
HIDDEN unsigned int acknowledge(int line, int devNum, devregarea_t* devRegisterArea, int devSemaphore);


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
        /* Processor Local Timer interrupt*/
        handlePLT();                            /* PLT interrupt line: 1 */
    } else if (cause & ITINTERRUPT) {
        /* Interval Timer interrupt */
        handlePseudoClock();                    /* Pseudo-clock interrupt line: 1 */
    } else if (cause & DISKINTERRUPT) {
        /* Disk interrupt */
        handleDeviceInterrupt(DISK_LINE);       /* Disk interrupt line: 3 */
    } else if (cause & FLASHINTERRUPT) {
        /* Flash interrupt */
        handleDeviceInterrupt(FLASH_LINE);      /* Flash interrupt line: 4 */
    } else if (cause & NETWORKINTERRUPT) {
        /* Network interrupt */
        handleDeviceInterrupt(NETWORK_LINE);    /* Network interrupt line: 5 */
    } else if (cause & PRINTERINTERRUPT) {
        /* Printer interrupt */
        handleDeviceInterrupt(PRINTER_LINE);    /* Printer interrupt line: 6 */
    } else if (cause & TERMINTERRUPT) {
        /* Terminal interrupt */
        handleDeviceInterrupt(TERMINAL_LINE);   /* Terminal interrupt line: 7 */
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
 *   devMap - Bitmask representing the active device interrupt
 * 
 * Returns: The device number (0-7) or -1 if not found
 */
HIDDEN int getDeviceNumber(unsigned int devMap) {

    /* Check each device and return its number if found */
	if(devMap & 0x00000001) {
		return PROCESSOR_LINE;          /* Return 0 */
    } else if (devMap & 0x00000002) {
		return PLTIMER_LINE;            /* Return 1 */
    } else if (devMap & 0x00000004) {
		return INTERVALTIMER_LINE;      /* Return 2 */
    } else if (devMap & 0x00000008) {
		return DISK_INT;                /* Return 3 */
    } else if (devMap & 0x00000010) {
		return FLASH_INT;               /* Return 4 */
    } else if (devMap & 0x00000020) {
		return NETWORK_INT;             /* Return 5 */
    } else if (devMap & 0x00000040) {
		return PRINTER_INT;             /* Return 6 */
    } else if (devMap & 0x00000080) {
		return TERMINAL_INT;            /* Return 7 */
    } else {
        PANIC();    /* No device found */
        return ERROR;                   /* Return -1 */
    }
}

/*
 * Acknowledges an interrupt for a given device and returns its status.
 * 
 * Parameters:
 *   line - The interrupt line number
 *   devNum - The device number
 * 
 * Returns: The status code of the device after acknowledgment
 */
HIDDEN unsigned int acknowledge(int line, int devNum, devregarea_t* devRegisterArea, int devSemaphore) {
    unsigned int status;

    /* Check if it's a terminal device */
    if (line == TERMINT) {

        /* Check if it's a transmit or receive interrupt */
        if (devRegisterArea->devreg[devSemaphore].t_transm_status & TRANSM_BIT) {

            /* Transmit interrupt */
            status = devRegisterArea->devreg[devSemaphore].t_transm_status;
            devRegisterArea->devreg[devSemaphore].t_transm_command = ACK;

        } else { /* Receive interrupt */

            status = devRegisterArea->devreg[devSemaphore].t_recv_status;
            devRegisterArea->devreg[devSemaphore].t_recv_command = ACK;
        }
    } else { /* Non-terminal devices */

        status = devRegisterArea->devreg[devSemaphore].d_status;
        devRegisterArea->devreg[devSemaphore].d_command = ACK;
    }
    return status;
}

/*
 * Handle device interrupts from any non-timer I/O device.
 * This is the main entry point called from interruptHandler.
 * 
 * Parameters:
 *   line - The interrupt line that was triggered (3-7)
 */
HIDDEN void handleDeviceInterrupt(int line) {
    
    /* Get interrupt device map */
    devregarea_t* devRegisterArea = (devregarea_t*) RAMBASEADDR;
    unsigned int devMap = devRegisterArea->interrupt_dev[line - MAPINT];    /* MAPINT = 3 */
    
    /* Find which device triggered the interrupt */
    int devNum = getDeviceNumber(devMap);
    
    /* Calculate device semaphore index */
    int devSemaphore = ((line - MAPINT) * DEV_PER_LINE) + devNum; /* MAPINT = 3 */
    
    /* Acknowledge interrupt and get status code */
    unsigned int status = acknowledge(line, devNum, devRegisterArea, devSemaphore);
    
    /* Perform V operation on semaphore to unblock any waiting process */
    pcb_PTR unblockedProcess = verhogen(&deviceSemaphores[devSemaphore]);

    /* Store status code in v0 register */
    if (unblockedProcess != NULL) {
        unblockedProcess->p_s.s_v0 = status;
    }
    
    /* Control is returned to interruptHandler, which will either
     * resume the current process or call the scheduler as needed */
}
