/******************************************************************************
 *
 * Module: sysSupport.c
 *
 * This module implements the Support Level's exception handlers for
 * non-TLB exceptions that are passed up from the Nucleus (Level 3).
 * It includes a general exception handler that determines the type of
 * exception and dispatches to either the SYSCALL exception handler
 * (for SYSCALLs 9 and above) or the Program Trap exception handler.
 *
 * Authors: Aryah Rao & Anish Reddy
 * Date: Current Date
 *
 *****************************************************************************/

#include "../h/sysSupport.h"
#include "../h/vmSupport.h"

/*----------------------------------------------------------------------------*/
/* Helper Function Prototypes (static functions) */
/*----------------------------------------------------------------------------*/
HIDDEN support_t *getCurrentSupportStruct();
HIDDEN void terminateCurrentProcess();
HIDDEN int handleDeviceIO(int deviceType, char *buffer, int length);
HIDDEN int handleTerminalIO(int operation, char *buffer);

/*----------------------------------------------------------------------------*/
/* Exception Handlers */
/*----------------------------------------------------------------------------*/

/******************************************************************************
 * Function: genExceptionHandler
 *
 * Description:
 *   This is the Support Level's general exception handler. It determines 
 *   the type of exception and dispatches to the appropriate sub-handler.
 *****************************************************************************/
void genExceptionHandler() {
    /* Get current process's Support Structure */
    support_t *supportStruct = getCurrentSupportStruct();
    
    if (supportStruct == NULL) {
        /* This should not happen - terminate the process */
        SYSCALL(TERMINATEPROCESS, 0, 0, 0);
        return;
    }
    
    /* Access the saved exception state */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
    
    /* Extract exception cause code from Cause register (bits 2-6) */
    unsigned int excCode = (exceptState->s_cause & CAUSE_EXCCODE_MASK) >> CAUSE_EXCCODE_SHIFT;
    
    /* Determine the type of exception and dispatch to appropriate handler */
    if (excCode == SYSCALLS) {
        /* SYSCALL exception (SYS9 and above) */
        syscallExceptionHandler();
    } else {
        /* Any other exception is a Program Trap */
        programTrapExceptionHandler();
    }
    
    /* Control should not reach here as handlers either return to user mode
       using LDST or terminate the process */
    SYSCALL(TERMINATEPROCESS, 0, 0, 0);
}

/******************************************************************************
 * Function: syscallExceptionHandler
 *
 * Description:
 *   Handles SYSCALL exceptions for SYSCALLs 9 and above. It retrieves the
 *   SYSCALL number from a0 register and performs the corresponding service.
 *****************************************************************************/
void syscallExceptionHandler() {
    /* Get current process's Support Structure */
    support_t *supportStruct = getCurrentSupportStruct();
    
    if (supportStruct == NULL) {
        /* This should not happen - terminate the process */
        SYSCALL(TERMINATEPROCESS, 0, 0, 0);
        return;
    }
    
    /* Access the saved exception state */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
    
    /* Retrieve SYSCALL number from a0 register */
    unsigned int sysNum = exceptState->s_a0;
    
    /* Handle based on SYSCALL number */
    switch (sysNum) {
        case TERMINATE: /* SYS9 - Process termination */
            terminateCurrentProcess();
            break;
            
        case GET_TOD: /* SYS10 - Get time of day */
            /* Get current time via nucleus SYS6 */
            exceptState->s_v0 = SYSCALL(GETCPUTIME, 0, 0, 0);
            break;
            
        case WRITEPRINTER: /* SYS11 - Write to printer */
            {
                char *buffer = (char *)exceptState->s_a1;
                int length = exceptState->s_a2;
                
                /* Validate buffer address is in user space (kuseg) */
                if ((unsigned int)buffer >= 0x80000000) {
                    /* Invalid address - terminate the process */
                    terminateCurrentProcess();
                    return;
                }
                
                /* Perform printer I/O operation */
                exceptState->s_v0 = handleDeviceIO(PRNTINT, buffer, length);
            }
            break;
            
        case WRITETERMINAL: /* SYS12 - Write to terminal */
            {
                char *buffer = (char *)exceptState->s_a1;
                
                /* Validate buffer address is in user space (kuseg) */
                if ((unsigned int)buffer >= 0x80000000) {
                    /* Invalid address - terminate the process */
                    terminateCurrentProcess();
                    return;
                }
                
                /* Perform terminal write operation */
                exceptState->s_v0 = handleTerminalIO(0, buffer); /* 0 for write */
            }
            break;
            
        case READTERMINAL: /* SYS13 - Read from terminal */
            {
                char *buffer = (char *)exceptState->s_a1;
                
                /* Validate buffer address is in user space (kuseg) */
                if ((unsigned int)buffer >= 0x80000000) {
                    /* Invalid address - terminate the process */
                    terminateCurrentProcess();
                    return;
                }
                
                /* Perform terminal read operation */
                exceptState->s_v0 = handleTerminalIO(1, buffer); /* 1 for read */
            }
            break;
            
        default:
            /* SYSCALLs 14-20 are for later phases */
            if (sysNum >= DISK_GET && sysNum <= VSEMVIRT) {
                /* These are implemented in later phases - terminate for now */
                terminateCurrentProcess();
            } else {
                /* Unknown SYSCALL - terminate the process */
                terminateCurrentProcess();
            }
            break;
    }
    
    /* Increment PC to next instruction */
    exceptState->s_pc += 4;
    
    /* Return to user mode */
    LDST(exceptState);
}

/******************************************************************************
 * Function: programTrapExceptionHandler
 *
 * Description:
 *   Handles Program Trap exceptions by releasing any held semaphores and
 *   terminating the process.
 *****************************************************************************/
void programTrapExceptionHandler() {
    /* Handle any pending semaphores before terminating */
    /* For Phase 3, we only have the Swap Pool semaphore to worry about */
    /* If the current process holds the Swap Pool semaphore, release it */
    extern int swapPoolSemaphore;
    if (swapPoolSemaphore <= 0) {
        /* If semaphore value is <= 0, it might be held by this process */
        SYSCALL(VERHOGEN, &swapPoolSemaphore, 0, 0);
    }
    
    /* Terminate the process */
    terminateCurrentProcess();
}

/*----------------------------------------------------------------------------*/
/* Helper Functions */
/*----------------------------------------------------------------------------*/

/******************************************************************************
 * Function: getCurrentSupportStruct
 *
 * Description:
 *   Gets the Support Structure for the current process via SYS8.
 *
 * Returns:
 *   Pointer to the current process's Support Structure.
 *****************************************************************************/
HIDDEN support_t *getCurrentSupportStruct() {
    return (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
}

/******************************************************************************
 * Function: terminateCurrentProcess
 *
 * Description:
 *   Terminates the current process using SYS2.
 *****************************************************************************/
HIDDEN void terminateCurrentProcess() {
    SYSCALL(TERMINATEPROCESS, 0, 0, 0);
}

/******************************************************************************
 * Function: handleDeviceIO
 *
 * Description:
 *   Handles I/O operations for non-terminal devices.
 *
 * Parameters:
 *   deviceType - Type of device (e.g., PRNTINT for printer)
 *   buffer - Pointer to data buffer
 *   length - Length of data to transfer
 *
 * Returns:
 *   Number of characters transferred or error code
 *****************************************************************************/
HIDDEN int handleDeviceIO(int deviceType, char *buffer, int length) {
    int i, status;
    int deviceSemaphoreIndex;
    unsigned int commandWord;
    
    /* Calculate device semaphore index - assuming device 0 */
    deviceSemaphoreIndex = (deviceType - DISKINT) * DEVPERINT;
    
    /* Acquire the device semaphore for mutual exclusion */
    SYSCALL(PASSEREN, &deviceSemaphores[deviceSemaphoreIndex], 0, 0);
    
    /* Access the device register area */
    devregarea_t *devRegs = (devregarea_t *)RAMBASEADDR;
    
    /* For printer: transfer each character one by one */
    for (i = 0; i < length && buffer[i] != '\0'; i++) {
        /* Prepare the command word: character in low byte, TRANSMIT in high byte */
        commandWord = (PRINTCHR << BYTELEN) | buffer[i];
        
        /* Write to the device's command register */
        devRegs->devreg[deviceSemaphoreIndex].d_command = commandWord;
        
        /* Wait for operation completion */
        status = SYSCALL(WAITIO, deviceType, 0, 0);
        
        /* Check for errors */
        if ((status & TERMSTATMASK) != PRINTCHR) {
            /* Error occurred - release semaphore and return count up to error */
            SYSCALL(VERHOGEN, &deviceSemaphores[deviceSemaphoreIndex], 0, 0);
            return i;
        }
    }
    
    /* Release the device semaphore */
    SYSCALL(VERHOGEN, &deviceSemaphores[deviceSemaphoreIndex], 0, 0);
    
    /* Return the number of characters transferred */
    return i;
}

/******************************************************************************
 * Function: handleTerminalIO
 *
 * Description:
 *   Handles I/O operations for the terminal device.
 *
 * Parameters:
 *   operation - 0 for write, 1 for read
 *   buffer - Pointer to data buffer
 *
 * Returns:
 *   Number of characters transferred or error code
 *****************************************************************************/
HIDDEN int handleTerminalIO(int operation, char *buffer) {
    int i, status;
    int deviceSemaphoreIndex;
    unsigned int commandWord;
    
    /* Calculate terminal device semaphore index - assuming device 0 */
    deviceSemaphoreIndex = (TERMINT - DISKINT) * DEVPERINT;
    
    /* For read, use the second set of terminal semaphores */
    if (operation == 1) {
        deviceSemaphoreIndex += DEVPERINT; /* Offset for receive vs transmit registers */
    }
    
    /* Acquire the device semaphore for mutual exclusion */
    SYSCALL(PASSEREN, &deviceSemaphores[deviceSemaphoreIndex], 0, 0);
    
    /* Access the device register area */
    devregarea_t *devRegs = (devregarea_t *)RAMBASEADDR;
    
    if (operation == 0) { /* Write operation */
        /* Transfer characters one by one until null terminator */
        for (i = 0; buffer[i] != '\0'; i++) {
            /* Prepare the command word: character in low byte, TRANSMIT in high byte */
            commandWord = (TRANSMIT << BYTELEN) | buffer[i];
            
            /* Write to the device's transmit command register */
            devRegs->devreg[deviceSemaphoreIndex].t_transm_command = commandWord;
            
            /* Wait for operation completion */
            status = SYSCALL(WAITIO, TERMINT, 0, 0);
            
            /* Check for errors */
            if ((status & TERMSTATMASK) != OKCHARTRANS) {
                /* Error occurred - release semaphore and return count up to error */
                SYSCALL(VERHOGEN, &deviceSemaphores[deviceSemaphoreIndex], 0, 0);
                return i;
            }
        }
    } else { /* Read operation */
        /* Read characters one by one until newline or buffer limit reached */
        for (i = 0; i < 128; i++) { /* Arbitrary buffer size limit */
            /* Send read command to terminal */
            devRegs->devreg[deviceSemaphoreIndex].t_recv_command = RECEIVE;
            
            /* Wait for operation completion */
            status = SYSCALL(WAITIO, TERMINT, 0, 1); /* 1 indicates read operation */
            
            /* Extract received character from status */
            char receivedChar = status & 0xFF;
            
            /* Check for errors */
            if ((status & TERMSTATMASK) != OKCHARTRANS) {
                /* Error occurred - release semaphore and return count */
                SYSCALL(VERHOGEN, &deviceSemaphores[deviceSemaphoreIndex], 0, 0);
                return i;
            }
            
            /* Store character in buffer */
            buffer[i] = receivedChar;
            
            /* Stop at newline */
            if (receivedChar == '\n') {
                i++; /* Include the newline in the count */
                break;
            }
        }
    }
    
    /* Release the device semaphore */
    SYSCALL(VERHOGEN, &deviceSemaphores[deviceSemaphoreIndex], 0, 0);
    
    /* Return the number of characters transferred */
    return i;
}