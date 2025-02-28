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

/* Forward declarations for helper functions */
HIDDEN unsigned int acknowledgeDevice(int* semaddr);
HIDDEN int getHighestPriorityInt(unsigned int cause);
HIDDEN void handlePseudoClock();
HIDDEN void handleNonTerminal(int line, int device);
HIDDEN void handleTerminal(int device);
HIDDEN void handlePLT();

/* Current TOD */
HIDDEN cpu_t currentTime;
HIDDEN unsigned int quantumLeft;

/* Main interrupt handler - Called when an interrupt occurs
 * According to the principles of operation, this handler needs to:
 * 1. Save the current process state from the BIOS area
 * 2. Update CPU time for the current process
 * 3. Identify which interrupt occurred
 * 4. Call the appropriate interrupt handler
 * 5. Schedule the next process or resume the current one
 */
void interruptHandler() {
    /* Get old processor state from BIOS area (0x20000000) as specified in PandOS spec */
    state_t* interruptState = (state_t*) BIOSDATAPAGE;

    /* Save the current process's CPU time by calculating elapsed time since process started
     * This follows the CPU accounting requirements in PandOS spec section 3.4 */
    if (currentProcess != NULL) {
        /* Deep copy state from BIOS data page to current process state - ONLY if currentProcess exists */
        copyState(&currentProcess->p_s, interruptState);
        
        STCK(currentTime);  /* Read current time using STCK macro */
        currentProcess->p_time += (currentTime - startTOD); /* Add elapsed time to process's accumulated CPU time */
    }
    
    /* Calculate quantum left before interrupt occurred */
    quantumLeft = getTIMER();

    /* Get the interrupt cause, which is the interrupt that triggered this handler */
    unsigned int cause = (interruptState->s_cause & CAUSE_IP_MASK) >> CAUSE_IP_SHIFT; /* Interrupt Pending bits */
    
    /* Get the highest priority interrupt */
    int line = getHighestPriorityInt(cause);
    
    /* Process the interrupt based on its type
     * Each case follows handling as specified in PandOS spec section 3.7 */
    int dev = 0;
    switch (line) {
        case TIMER_INT:
            /* Processor Local Timer interrupt (quantum expired) */
            handlePLT();
            break;
            
        case CLOCK_INT:
            /* Interval Timer interrupt (for pseudo-clock)
             * According to PandOS spec section 3.7.2, this wakes up processes waiting on the clock */
            handlePseudoClock();
            break;
            
        case DISK_INT:
        case FLASH_INT:
        case NET_INT:
        case PRINTER_INT:
            /* Non-terminal device interrupts
             * For each device on this interrupt line, check if it generated an interrupt */
            for (dev = 0; dev < DEV_PER_INT; dev++) {
                /* Check if this specific device generated an interrupt 
                 * Access device register status to check for pending interrupts */
                device_t* device = (device_t*) DEV_REG_ADDR(INT_DEVICENUMBER((line - 3) * DEV_PER_INT + dev));
                if (device->d_status & 0xFF) {  /* Non-zero status indicates interrupt pending */
                    handleNonTerminal(line, dev);
                }
            }
            break;
            
        case TERMINAL_INT:
            /* Terminal device interrupts - both read and write operations
             * As per PandOS spec section 3.7.5, terminals have two sub-devices (receiver & transmitter) */
            for (dev = 0; dev < DEV_PER_INT; dev++) {
                /* Get device register to check for pending interrupts */
                device_t* term = (device_t*) DEV_REG_ADDR(INT_DEVICENUMBER((TERMINAL_INT - 3) * DEV_PER_INT + dev));
                
                /* Check if this terminal has any pending interrupts */
                if ((term->t_transm_status & TERM_STATUS_MASK) != TERM_TX_READY || 
                    (term->t_recv_status & TERM_STATUS_MASK) != TERM_RX_READY) {
                    handleTerminal(dev);
                }
            }
            break;
            
        default:
            /* Unknown interrupt type - system error */
            PANIC();
    }

    /* Resume execution of current process or call scheduler
     * According to PandOS spec section 3.7, if an interrupt doesn't trigger
     * a process switch, the current process continues execution */
    if (currentProcess != NULL) {
        loadProcessState(&currentProcess->p_s, quantumLeft);
    } else {
        /* No current process - call scheduler to select next process */
        scheduler();
    }
}

/* Helper function to acknowledge a device interrupt and get its status
 * According to PandOS spec section 3.7.3, each I/O operation must be acknowledged
 * by writing ACK command to the device's command register
 *
 * Parameters:
 *   semaddr - Pointer to semaphore for this device
 * 
 * Returns: Device status value, which contains operation result codes
 */
HIDDEN unsigned int acknowledgeDevice(int* semaddr) {
    /* Calculate device register address from semaphore index
     * This computation converts a semaphore index to a physical device register address
     * INT_DEVICENUMBER extracts device number from semaphore index (per PandOS spec) */
    memaddr deviceRegisterBase = (memaddr) DEV_REG_ADDR(INT_DEVICENUMBER(*semaddr));
    device_t* device = (device_t*) deviceRegisterBase;
    
    /* Read the device status register before acknowledging
     * According to PandOS spec section 3.7.3, we must read status before ACK */
    unsigned int status = device->d_status;
    
    /* Write ACK command to device command register to acknowledge interrupt
     * This follows PandOS spec requirement for interrupt acknowledgment */
    device->d_command = ACK;
    
    /* Return status to caller (will be given to waiting process) */
    return status;
}

/* Determine highest priority interrupt from the cause register bits
 * According to PandOS spec section 3.7, interrupts have priorities
 * with higher interrupt lines having higher priority
 *
 * Parameters:
 *   cause - The interrupt pending bits from the cause register
 * 
 * Returns: The highest priority interrupt line number (7=highest, 0=lowest)
 */
HIDDEN int getHighestPriorityInt(unsigned int cause) {
    int i;
    /* Scan through all interrupt lines from highest (7) to lowest (0)
     * CAUSE_IP(i) generates the bit mask for interrupt line i */
    for (i = 7; i >= 0; i--) {
        if (cause & CAUSE_IP(i)) 
            return i;  /* Return the highest priority interrupt found */
    }
    return -1;  /* No interrupts pending - should not happen in this context */
}

/* Handle non-terminal device interrupts (disk, flash, network, printer)
 * According to PandOS spec section 3.7.4, this should:
 * 1. Acknowledge the interrupt
 * 2. Update the semaphore
 * 3. Unblock any waiting process
 *
 * Parameters:
 *   line - Interrupt line number (3-6)
 *   device - Device number within the line (0-7)
 */
HIDDEN void handleNonTerminal(int line, int device) {
    /* Calculate semaphore index using the formula from PandOS spec:
     * ((line - 3) * 8) + device
     * This formula maps each device to a unique semaphore:
     * - Disk starts at index 0 (line=3)
     * - Flash starts at index 8 (line=4)
     * - Network starts at index 16 (line=5)
     * - Printer starts at index 24 (line=6)
     * Each line has 8 devices, so we multiply by 8 and add device number */
    int sem_index = ((line - 3) * DEV_PER_INT) + device;
    
    /* Acknowledge device interrupt and get status
     * As per PandOS spec section 3.7.3, this returns operation status code */
    unsigned int status = acknowledgeDevice(&deviceSemaphores[sem_index]);
    
    /* If any process is blocked on this device, unblock it
     * According to PandOS spec section 3.7.4, the status is returned to the process */
    pcb_PTR p = removeBlocked(&deviceSemaphores[sem_index]);
    if (p != NULL) {
        softBlockCount--;  /* One less blocked process */
        
        /* Store device status in v0 register - this is the system call return value
         * As specified in PandOS spec section 3.7.4 */
        p->p_s.s_v0 = status;
        
        /* Add the unblocked process to the ready queue */
        insertProcQ(&readyQueue, p);
    }
    
    /* Perform V operation on device semaphore (increment)
     * According to PandOS spec section 3.7.4, this completes the I/O operation */
    deviceSemaphores[sem_index]++;
}

/* Handle terminal device interrupts for both read and write operations
 * According to PandOS spec section 3.7.5, terminals have separate
 * receiver and transmitter sub-devices that generate distinct interrupts
 *
 * Parameters:
 *   device - Terminal device number (0-7)
 */
HIDDEN void handleTerminal(int device) {
    /* Calculate the correct terminal device address using the formula:
     * 0x10000054 + ((7 - 3) * 0x80) + (device * 0x10)
     * where 7 is TERMINAL_INT and 0x10 is the device register size */
    memaddr termBase = DEV_REG_START + ((TERMINAL_INT - 3) * DEV_PER_INT * DEV_REG_SIZE) + 
                        (device * DEV_REG_SIZE);
    device_t* term = (device_t*) termBase;
    unsigned int status;
    
    /* Check for transmit (write) interrupt FIRST - important for UMPS3
     * Check transmitter status - if not READY, there's an interrupt pending */
    if ((term->t_transm_status & TERM_STATUS_MASK) != TERM_TX_READY) {
        /* Get the status before acknowledging */
        status = term->t_transm_status;
        
        /* Acknowledge the transmission interrupt explicitly */
        term->t_transm_command = ACK;
        
        /* Calculate the transmitter semaphore index */
        int tx_sem_idx = TERM_TX_SEM(device);
        
        /* Unblock any process waiting on this terminal's transmitter */
        pcb_PTR p = removeBlocked(&deviceSemaphores[tx_sem_idx]);
        if (p != NULL) {
            softBlockCount--;
            /* Store the status code in v0 - this will be returned to the process 
             * that was waiting on this I/O operation */
            p->p_s.s_v0 = status;
            insertProcQ(&readyQueue, p);
        }
        
        /* Perform V operation on the terminal transmitter semaphore */
        deviceSemaphores[tx_sem_idx]++;
    }
    
    /* Now check for receive (read) interrupt
     * Check receiver status - if not READY, there's an interrupt pending */
    if ((term->t_recv_status & TERM_STATUS_MASK) != TERM_RX_READY) {
        /* Get the status before acknowledging */
        status = term->t_recv_status;
        
        /* Acknowledge the receiver interrupt explicitly */
        term->t_recv_command = ACK;
        
        /* Calculate the receiver semaphore index */
        int rx_sem_idx = TERM_RX_SEM(device);
        
        /* Unblock any process waiting on this terminal's receiver */
        pcb_PTR p = removeBlocked(&deviceSemaphores[rx_sem_idx]);
        if (p != NULL) {
            softBlockCount--;
            /* Store the status code in v0 - this will be returned to the process 
             * that was waiting on this I/O operation */
            p->p_s.s_v0 = status;
            insertProcQ(&readyQueue, p);
        }
        
        /* Perform V operation on the terminal receiver semaphore */
        deviceSemaphores[rx_sem_idx]++;
    }
}

/* DONE */
/* PLT interrupt handler: Qunatum Expired*/
HIDDEN void handlePLT() {
    /* Acknowledge interrupt */
    setTIMER(CLOCKINTERVAL);

    if (currentProcess != NULL) {
        /* Current Process already updated with CPU time, process state, etc. in interruptHandler */

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