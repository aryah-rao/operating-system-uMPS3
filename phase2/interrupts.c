/**************************** interrupts.c **********************************
*
* The interrupt handling module for the PandOS Operating System.
* Handles all device interrupts and timer interrupts.
*
* Functions:
* - interruptHandler: Main interrupt dispatcher
* - handleDeviceInterrupt: Device interrupt handler
* - handleTerminal: Terminal device handler
* - handleNonTerminal: Non-terminal device handler
* - handlePseudoClock: Interval timer handler
*
*********************************************************************/

#include "../h/interrupts.h"

/* Helper function to acknowledge device interrupts */
HIDDEN unsigned int acknowledgeDevice(int* semaddr) {
    /* Get device register base */
    memaddr deviceRegisterBase = (memaddr) DEV_REG_ADDR(INT_DEVICENUMBER(*semaddr));
    device_t* device = (device_t*) deviceRegisterBase;
    
    /* Get status and acknowledge */
    unsigned int status = device->d_status;
    device->d_command = ACK;
    
    return status;
}

/* Helper function to find highest priority interrupt */
HIDDEN int getHighestPriorityInt(unsigned int cause) {
    int i;
    /* Check each interrupt bit from highest to lowest priority */
    for (i = 7; i >= 0; i--) {
        if (cause & CAUSE_IP(i)) return i;
    }
    return -1;  /* No interrupts pending */
}

/* Pseudo-clock timer handler */
HIDDEN void handlePseudoClock() {
    pcb_PTR p;
    
    /* Acknowledge interrupt by loading new interval */
    LDIT(QUANTUM);
    
    /* Wake up all processes blocked on pseudo-clock */
    while ((p = removeBlocked(&deviceSemaphores[DEVICE_COUNT])) != NULL) {
        softBlockCount--;
        insertProcQ(&readyQueue, p);
    }
    
    /* Reset pseudo-clock semaphore */
    deviceSemaphores[DEVICE_COUNT] = 0;
}

/* Non-terminal device interrupt handler */
HIDDEN void handleNonTerminal(int line, int device) {
    /* Calculate semaphore index */
    int sem_index = ((line - 3) * DEV_PER_INT) + device;
    
    /* Acknowledge device and get status */
    unsigned int status = acknowledgeDevice(&deviceSemaphores[sem_index]);
    
    /* Unblock one process if any are blocked on this device */
    pcb_PTR p = removeBlocked(&deviceSemaphores[sem_index]);
    if (p != NULL) {
        softBlockCount--;
        p->p_s.s_v0 = status;  /* Return device status to the process */
        insertProcQ(&readyQueue, p);
    }
    
    /* V operation on device semaphore */
    deviceSemaphores[sem_index]++;
}

/* Terminal device interrupt handler */
HIDDEN void handleTerminal(int device) {
    /* Get terminal register base */
    device_t* term = (device_t*) DEV_REG_ADDR(INT_DEVICENUMBER(device));
    unsigned int status;
    
    /* Check for receive interrupt (higher priority) */
    if ((term->d_status & DEV_TERM_STATUS) != DEV_TERM_READY) {
        /* Handle receiver interrupt */
        status = acknowledgeDevice(&deviceSemaphores[TERM_RX_SEM(device)]);
        pcb_PTR p = removeBlocked(&deviceSemaphores[TERM_RX_SEM(device)]);
        if (p != NULL) {
            softBlockCount--;
            p->p_s.s_v0 = status;
            insertProcQ(&readyQueue, p);
        }
        deviceSemaphores[TERM_RX_SEM(device)]++;
    }
    
    /* Check for transmit interrupt */
    if ((term->d_data0 & DEV_TERM_STATUS) != DEV_TERM_READY) {
        /* Handle transmitter interrupt */
        status = acknowledgeDevice(&deviceSemaphores[TERM_TX_SEM(device)]);
        pcb_PTR p = removeBlocked(&deviceSemaphores[TERM_TX_SEM(device)]);
        if (p != NULL) {
            softBlockCount--;
            p->p_s.s_v0 = status;
            insertProcQ(&readyQueue, p);
        }
        deviceSemaphores[TERM_TX_SEM(device)]++;
    }
}

/* Main interrupt handler */
void interruptHandler() {
    /* Get old processor state */
    state_t* oldState = (state_t*) INT_OLDAREA;
    
    /* Save CPU time for current process */
    if (currentProcess != NULL) {
        cpu_t currentTime;
        STCK(currentTime);
        currentProcess->p_time += (currentTime - startTOD);
    }
    
    /* Get interrupt cause */
    unsigned int cause = (oldState->s_cause & CAUSE_IP_MASK) >> 8;
    
    /* Get highest priority interrupt */
    int line = getHighestPriorityInt(cause);
    
    /* Handle different types of interrupts */
    int dev = 0;
    switch (line) {
        case TIMER_INT:
            /* Processor Local Timer interrupt */
            if (currentProcess != NULL) {
                insertProcQ(&readyQueue, currentProcess);
                currentProcess = NULL;
            }
            break;
            
        case CLOCK_INT:
            /* Interval Timer interrupt */
            handlePseudoClock();
            break;
            
        case DISK_INT:
        case FLASH_INT:
        case NET_INT:
        case PRINTER_INT:
            /* Non-terminal device interrupts */
            for (dev = 0; dev < DEV_PER_INT; dev++) {
                if (cause & CAUSE_IP(line)) {
                    handleNonTerminal(line, dev);
                }
            }
            break;
            
        case TERMINAL_INT:
            /* Terminal device interrupts */
            for (dev = 0; dev < DEV_PER_INT; dev++) {
                if (cause & CAUSE_IP(TERMINAL_INT)) {
                    handleTerminal(dev);
                }
            }
            break;
            
        default:
            PANIC();
    }
    
    /* Update system time */
    STCK(startTOD);
    
    /* If there was a running process, restore it */
    if (currentProcess != NULL) {
        loadProcessState(&currentProcess->p_s);
    } else {
        /* Otherwise call scheduler */
        scheduler();
    }
}

