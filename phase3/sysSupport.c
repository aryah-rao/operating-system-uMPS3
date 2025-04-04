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
#include "../h/const.h"
#include "../h/types.h"

/*----------------------------------------------------------------------------*/
/* Global Variables */
/*----------------------------------------------------------------------------*/
/*
 * None expected to be globally visible from this module. Static variables
 * can be declared within the functions if needed for internal state.
 */

/*----------------------------------------------------------------------------*/
/* Helper Function Prototypes (HIDDEN functions) */
/*----------------------------------------------------------------------------*/
/*
 * Potentially functions to help with specific tasks within the handlers.
 * For example, a helper to extract the SYSCALL number.
 */
HIDDEN void terminateUProc();
HIDDEN unsigned int getTimeOfDay();
HIDDEN int writePrinter(char *buffer, int length);
HIDDEN int deviceWrite(int deviceType, char *buffer, int length);
HIDDEN int terminalOperation(int operation, char *buffer);
HIDDEN support_t *getCurrentSupportStruct();
HIDDEN int validateUserAddress(void *address);

/* Added helper function prototypes */
void setInterrupts(int onOff);           /* Enable or disable interrupts based on parameter */
void resumeState(state_t *state);          /* Load processor state */

/*----------------------------------------------------------------------------*/
/* Exception Handlers */
/*----------------------------------------------------------------------------*/

/******************************************************************************
 *
 * Function: genExceptionHandler
 *
 * Description:
 *   This is the Support Level's general exception handler. It is the entry
 *   point for all non-TLB exceptions that the Nucleus has passed up for
 *   handling (i.e., when the process's p_supportStruct is not NULL).
 *   This handler examines the Cause register in the saved exception state
 *   to determine the specific type of exception and then dispatches
 *   control to the appropriate sub-handler (SYSCALL or Program Trap).
 *
 * Parameters:
 *   None. The saved exception state is expected to be in a location
 *   accessible via the Current Process's Support Structure.
 *
 * Return Value:
 *   None. This function should not return in the traditional sense. It will
 *   either dispatch to another handler or terminate the process.
 *
 * Implementation Steps (High-Level):
 * 1. **Get the saved exception state:** Access the `sup_exceptState[GENERALEXCEPT]`
 *    field from the Current Process's Support Structure.
 *
 * 2. **Determine the exception cause:** Extract the exception code from the
 *    `s_cause` register of the saved exception state. Refer to [Section 3.3-pops]
 *    and [Table I.1-pops] for possible exception codes.
 *
 * 3. **Dispatch based on exception type:**
 *    - If the exception is a **SYSCALL exception (ExcCode value corresponding to SYSCALL)**,
 *      call the `syscallExceptionHandler`.
 *    - If the exception is a **Program Trap exception (any other non-TLB related
 *      ExcCode)**, call the `programTrapExceptionHandler`.
 *
 * 4. **Handle unexpected exceptions:** If the exception cause is not recognized
 *    or should not have been passed up, you might want to implement a default
 *    behavior, potentially terminating the process.
 *
 * 5. **Ensure proper context switching:** The called sub-handlers are responsible
 *    for returning control to the user process using `LDST` with the (potentially
 *    modified) saved exception state.
 *
 * Notes:
 *   - This handler runs in kernel-mode with interrupts enabled.
 *   - The saved exception state contains the processor state at the time of the
 *     exception.
 *   - The Support Structure's `sup_exceptContext[GENERALEXCEPT]` field should have
 *     been initialized by the InstantiatorProcess with the entry point and stack
 *     pointer for this handler.
 *
 * Cross References:
 *   - [Section 3.7] Pass Up or Die
 *   - [Section 4.6] The Support Level General Exception Handler
 *   - [Section 4.7] The SYSCALL Exception Handler
 *   - [Section 4.8] The Program Trap Exception Handler
 *   - [Section 3.3-pops] Cause Register Description
 *   - [Table I.1-pops] Cause Register Status Codes
 *****************************************************************************/
void genExceptionHandler() {
    // High-level implementation:
    // 1. Get the current process's Support Structure.
    // 2. Access the saved exception state for general exceptions.
    // 3. Extract the exception code from the Cause register.
    // 4. Use a switch statement or if-else chain to dispatch based on the exception code.
    //    - If SYSCALL exception, call syscallExceptionHandler.
    //    - If Program Trap exception, call programTrapExceptionHandler.
    // 5. Handle any unexpected or invalid exception codes.
    
    /* Get the current process's Support Structure */
    support_t *supportStruct = getCurrentSupportStruct();
    
    if (supportStruct == NULL) {
        /* This shouldn't happen; terminate if it does */
        terminateUProc();
        return;
    }
    
    /* Access the saved exception state */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
    
    /* Extract exception code from Cause register */
    unsigned int cause = (exceptState->s_cause & CAUSE_EXCCODE_MASK) >> CAUSE_EXCCODE_SHIFT;
    
    if (cause == SYSCALLS) {
        /* SYSCALL exception */
        syscallExceptionHandler();
    }

    /* Any other exception is a Program Trap */
    programTrapExceptionHandler();
}

/******************************************************************************
 *
 * Function: syscallExceptionHandler
 *
 * Description:
 *   This handler is invoked by the general exception handler when a SYSCALL
 *   exception with a number 9 or greater occurs in a user process that has
 *   a non-NULL Support Structure. This handler retrieves the SYSCALL number
 *   from register a0 in the saved exception state and performs the
 *   corresponding service.
 *
 * Parameters:
 *   None. The saved exception state is expected to be accessible as described
 *   in the `genExceptionHandler`.
 *
 * Return Value:
 *   None. This function should not return directly. It should update the
 *   return value in the saved exception state (register v0) and then
 *   return control to the user process using `LDST`.
 *
 * Implementation Steps (High-Level):
 * 1. **Get the saved exception state:** Access the `sup_exceptState[GENERALEXCEPT]`
 *    field from the Current Process's Support Structure.
 *
 * 2. **Retrieve the SYSCALL number:** Obtain the value from register a0 (`s_reg`)
 *    in the saved exception state.
 *
 * 3. **Dispatch based on SYSCALL number:** Use a switch statement or if-else
 *    chain to implement the services for SYSCALLs 9 through 13 (and potentially
 *    others in later phases).
 *    - **SYS9 (TERMINATE)**: Call a function to terminate the current
 *      process (similar to the SYS2 Nucleus call, but potentially involving
 *      releasing Support Level resources first).
 *    - **SYS10 (GET TOD)**: Read the Time Of Day (TOD) clock value and
 *      store it in register v0 (`s_reg`) of the saved exception state.
 *    - **SYS11 (WRITE TO PRINTER)**: Implement the logic to write a
 *      string to the printer device associated with the process. This will likely
 *      involve interacting with device registers (gaining mutual exclusion using
 *      SYS3 on the printer semaphore, writing data and command, and then issuing
 *      SYS5 to wait for completion). Store the number of characters transmitted
 *      (or an error code) in v0.
 *    - **SYS12 (WRITE TO TERMINAL)**: Similar to SYS11, but for the terminal
 *      device.
 *    - **SYS13 (READ FROM TERMINAL)**: Implement the logic to read a
 *      line of input from the terminal device into a buffer in the user's address
 *      space. Validate the buffer address. Store the number of characters read
 *      (or an error code) in v0.
 *    - **SYS14 (DISK PUT), SYS15 (DISK GET), SYS16 (FLASH PUT), SYS17 (FLASH GET),
 *      SYS18 (DELAY), SYS19 (PSEMLOGICAL), SYS20 (VSEMLOGICAL)**: These SYSCALLs
 *      are introduced in later phases (Levels 5, 6, and 7). If a SYSCALL number
 *      corresponding to a later phase is received in Phase 3/Level 4, you might
 *      want to return an error or terminate the process.
 *
 * 4. **Increment the Program Counter (PC):** Before returning, increment the PC
 *    (`s_pc`) in the saved exception state by 4 to point to the instruction
 *    following the SYSCALL instruction.
 *
 * 5. **Return to user mode:** Use `LDST` to load the (potentially modified)
 *    processor state from the saved exception state, which will return control
 *    to the user process.
 *
 * Notes:
 *   - This handler runs in kernel-mode with interrupts enabled.
 *   - Be careful when accessing user-provided addresses (e.g., for strings in
 *     SYS11, SYS12, and SYS13). Address translation is active at this level,
 *     so these are virtual addresses. Ensure they are within the U-proc's
 *     logical address space. Invalid user addresses might lead to further
 *     exceptions (e.g., page faults). The requirements for handling invalid
 *     addresses are specified for some SYSCALLs (e.g., terminate on invalid
 *     read/write addresses for flash and terminal read).
 *   - For I/O operations, remember to handle device semaphores for mutual
 *     exclusion and use `SYS5` to wait for device completion.
 *
 * Cross References:
 *   - [Section 4.7] The SYSCALL Exception Handler
 *   - [Section 3.5.10] Returning from a SYSCALL Exception (Nucleus)
 *   - [Section 4.7.1] Terminate (SYS9)
 *   - [Section 4.7.2] Get TOD (SYS10)
 *   - [Section 4.7.3] Write To Printer (SYS11)
 *   - [Section 4.7.4] Write To Terminal (SYS12)
 *   - [Section 4.7.5] Read From Terminal (SYS13)
 *   - [Chapter 5] Phase 4 - Level 5: DMA Device Support (for DISK and FLASH SYSCALLs)
 *   - [Chapter 6] Phase 5 - Level 6: The Delay Facility (for DELAY SYSCALL)
 *   - [Chapter 7] Phase 6 - Level 7: Cooperating User Processes (for PSEMLOGICAL and VSEMLOGICAL SYSCALLs)
 *****************************************************************************/
void syscallExceptionHandler() {
// High-level implementation:
    // 1. Get the current process's Support Structure.
    // 2. Access the saved exception state for general exceptions.
    // 3. Retrieve the SYSCALL number from s_reg.
    // 4. Use a switch statement to handle SYSCALLs 9-13:
    //    - Case 9 (TERMINATE): Implement process termination logic.
    //    - Case 10 (GET TOD): Read TOD and store in s_reg.
    //    - Case 11 (WRITE TO PRINTER): Implement printer write logic using device semaphores and SYS5. Store status in s_reg.
    //    - Case 12 (WRITE TO TERMINAL): Implement terminal write logic using device semaphores and SYS5. Store status in s_reg.
    //    - Case 13 (READ FROM TERMINAL): Implement terminal read logic using device semaphores and SYS5. Store status in s_reg. Validate user address.
    //    - Default: Handle unknown SYSCALL numbers (e.g., return error or terminate).
    // 5. Increment s_pc by 4.
    // 6. Perform LDST to return to the user process.
    
    /* Get the current process's Support Structure */
    support_t *supportStruct = getCurrentSupportStruct();
    
    if (supportStruct == NULL) {
        /* This shouldn't happen; terminate if it does */
        terminateUProc();
        return;
    }
    
    /* Access the saved exception state */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
    
    /* Retrieve the SYSCALL number from a0 */
    int sysNum = exceptState->s_a0;
    
    /* Dispatch based on SYSCALL number */
    switch (sysNum) {
        case TERMINATE:
            /* SYS9: TERMINATE */
            terminateUProc();
            break;
            
        case GETTOD:
            /* SYS10: GET TOD */
            exceptState->s_v0 = getTimeOfDay();
            break;
            
        case WRITEPRINTER:
            /* SYS11: WRITE TO PRINTER */
            {
                char *buffer = (char *)exceptState->s_a1;
                int length = exceptState->s_a2;
                
                /* Validate buffer address */
                if (validateUserAddress(buffer)) {
                    exceptState->s_v0 = writePrinter(buffer, length);
                } else {
                    terminateUProc();
                }
            }
            break;
            
        case WRITETERMINAL:
            /* SYS12: WRITE TO TERMINAL */
            {
                char *buffer = (char *)exceptState->s_a1;
                
                /* Validate buffer address */
                if (validateUserAddress(buffer)) {
                    exceptState->s_v0 = terminalOperation(0, buffer); /* 0 for write */
                } else {
                    terminateUProc();
                }
            }
            break;
            
        case READTERMINAL:
            /* SYS13: READ FROM TERMINAL */
            {
                char *buffer = (char *)exceptState->s_a1;
                
                /* Validate buffer address */
                if (validateUserAddress(buffer)) {
                    exceptState->s_v0 = terminalOperation(1, buffer); /* 1 for read */
                } else {
                    terminateUProc();
                }
            }
            break;
            
        default:
            /* Unknown SYSCALL number */
            terminateUProc();
            break;
    }
    
    /* Increment PC to next instruction */
    exceptState->s_pc += WORDLEN;
    
    /* Return to user mode */
    resumeState(exceptState);
}

/******************************************************************************
 *
 * Function: programTrapExceptionHandler
 *
 * Description:
 *   This handler is invoked by the general exception handler when a Program
 *   Trap exception occurs in a user process that has a non-NULL Support
 *   Structure. The purpose of this handler is to terminate the offending
 *   process in an orderly manner.
 *
 * Parameters:
 *   None. The saved exception state is expected to be accessible as described
 *   in the `genExceptionHandler`.
 *
 * Return Value:
 *   None. This function should not return directly. It should initiate the
 *   process termination sequence.
 *
 * Implementation Steps (High-Level):
 * 1. **Get the saved exception state:** Access the `sup_exceptState[GENERALEXCEPT]`
 *    field from the Current Process's Support Structure.
 *
 * 2. **Release Support Level semaphores (if held):** Check if the process is
 *    currently holding mutual exclusion on any Support Level semaphores
 *    (e.g., the Swap Pool semaphore). If so, perform a `V` operation (using
 *    `SYS4`) on those semaphores to release them. You'll need a way to
 *    track which semaphores a process might be holding.
 *
 * 3. **Terminate the process:** Invoke the Nucleus's terminate process service
 *    (using `SYS2`).
 *
 * Notes:
 *   - This handler runs in kernel-mode with interrupts enabled.
 *   - Orderly termination is crucial to avoid deadlocks or resource leaks.
 *   - The saved exception state might contain information about the cause of
 *     the Program Trap (e.g., Reserved Instruction), but the requirement here
 *     is simply to terminate the process.
 *
 * Cross References:
 *   - [Section 4.8] The Program Trap Exception Handler
 *   - [Section 3.5.2] Terminate Process (SYS2)
 *   - [Section 4.7.1] Terminate (SYS9) (for the same operations)
 *   - [Section 3.5.4] Verhuagen (V) (SYS4)
 *****************************************************************************/
void programTrapExceptionHandler() {
// High-level implementation:
    // 1. Get the current process's Support Structure.
    // 2. Check if the process holds any Support Level semaphores (e.g., Swap Pool).
    // 3. If semaphores are held, perform SYS4 (V operation) to release them.
    // 4. Perform SYSCALL(TERMINATEPROCESS, 0, 0, 0) or the equivalent SYS2 call.
    
    /* Simply terminate the process - semaphore release is handled in terminateUProc() */
    terminateUProc();
}

/*----------------------------------------------------------------------------*/
/* Helper Functions (Implementation) */
/*----------------------------------------------------------------------------*/

/* Helper function to get the support structure for the current process */
HIDDEN support_t *getCurrentSupportStruct() {
    return (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
}

/* Helper function to validate user addresses */
HIDDEN int validateUserAddress(void *address) {
    /* Check if address is in user space (KUSEG) */
    return (address >= KUSEG && address < (KUSEG + PAGESIZE * UPROCPAGES));
}

/* Terminate the current process with proper cleanup */
HIDDEN void terminateUProc() {
    /* Clear the current proceess's pages in swap pool */
    clearSwapPoolEntries();

    /* Release swap pool mutex if held (idk how to do this) */

    
    /* Update master semaphore to indicate process termination */
    SYSCALL(VERHOGEN, &masterSema4, 0, 0);

    /* Terminate the process */
    SYSCALL(TERMINATEPROCESS, 0, 0, 0);
}

/* Get the current time of day */
HIDDEN unsigned int getTimeOfDay() {
    /* Use the Nucleus's GETCPUTIME syscall */
    return SYSCALL(GETCPUTIME, 0, 0, 0);
}

/* Write to printer device */
HIDDEN int writePrinter(char *buffer, int length) {
    return deviceWrite(PRNTINT, buffer, length);
}

/* Generic device write operation */
HIDDEN int deviceWrite(int deviceType, char *buffer, int length) {
    int i = 0;
    int status = 0;
    int deviceSemIndex;
    
    /* Calculate device semaphore index - assuming device 0 */
    deviceSemIndex = (deviceType - DISKINT) * DEVPERINT;
    
    /* Acquire the device semaphore */
    SYSCALL(PASSEREN, &deviceMutex[deviceSemIndex], 0, 0);
    
    /* Get device register area */
    devregarea_t *devRegArea = (devregarea_t *)RAMBASEADDR;
    
    /* Write each character */
    for (i = 0; i < length && buffer[i] != '\0'; i++) {
        /* Prepare command word: character in low byte, command in high byte */
        unsigned int command;
        if (deviceType == PRNTINT) {
            command = (PRINTCHR << BYTELEN) | buffer[i];
        } else {
            command = (TRANSMIT << BYTELEN) | buffer[i];
        }
        
        /* Write to device register */
        if (deviceType == PRNTINT) {
            devRegArea->devreg[deviceSemIndex].d_command = command;
        } else {
            devRegArea->devreg[deviceSemIndex].t_transm_command = command;
        }
        
        /* Wait for operation to complete */
        status = SYSCALL(WAITIO, deviceType, 0, 0);
        
        /* Check for errors */
        if ((deviceType == PRNTINT && (status & DSTATUS) != DCHARMASK) ||
            (deviceType == TERMINT && (status & TERMSTATMASK) != OKCHARTRANS)) {
            /* Error occurred */
            break;
        }
    }
    
    /* Release the device semaphore */
    SYSCALL(VERHOGEN, &deviceMutex[deviceSemIndex], 0, 0);
    
    /* Return number of characters transmitted */
    return i;
}

/* Handle terminal operations (read or write) */
HIDDEN int terminalOperation(int operation, char *buffer) {
    int i = 0;
    int status = 0;
    int deviceSemIndex;
    
    /* Calculate terminal device semaphore index - assuming terminal 0 */
    deviceSemIndex = (TERMINT - DISKINT) * DEVPERINT;
    
    /* For read operations, use a different semaphore */
    if (operation == 1) {
        deviceSemIndex += DEVPERINT;
    }
    
    /* Acquire the device semaphore */
    SYSCALL(PASSEREN, &deviceMutex[deviceSemIndex], 0, 0);
    
    /* Get device register area */
    devregarea_t *devRegArea = (devregarea_t *)RAMBASEADDR;
    
    if (operation == 0) {
        /* Write operation */
        for (i = 0; buffer[i] != '\0'; i++) {
            /* Prepare command word: character in low byte, TRANSMIT in high byte */
            unsigned int command = (TRANSMIT << BYTELEN) | buffer[i];
            
            /* Write to transmit command register */
            devRegArea->devreg[deviceSemIndex].t_transm_command = command;
            
            /* Wait for operation to complete */
            status = SYSCALL(WAITIO, TERMINT, 0, 0);
            
            /* Check for errors */
            if ((status & TERMSTATMASK) != OKCHARTRANS) {
                /* Error occurred */
                break;
            }
        }
    } else {
        /* Read operation */
        for (i = 0; i < MAXSTRLENG; i++) {
            /* Send read command to terminal */
            devRegArea->devreg[deviceSemIndex].t_recv_command = RECEIVE;
            
            /* Wait for operation to complete */
            status = SYSCALL(WAITIO, TERMINT, 0, 1);
            
            /* Extract character from status */
            char c = status & CHARMASKED;
            
            /* Check for errors */
            if ((status & TERMSTATMASK) != OKCHARTRANS) {
                /* Error occurred */
                break;
            }
            
            /* Store character in buffer */
            buffer[i] = c;
            
            /* Stop at newline */
            if (c == '\n') {
                i++;  /* Include the newline in the count */
                break;
            }
        }
    }
    
    /* Release the device semaphore */
    SYSCALL(VERHOGEN, &deviceMutex[deviceSemIndex], 0, 0);
    
    /* Return number of characters processed */
    return i;
}

/* Done */
/* ========================================================================
 * Function: setInterrupts
 *
 * Description: Enables or disables interrupts by modifying the status register
 *              based on the onOff parameter.
 * 
 * Parameters:
 *              toggle - ON to enable interrupts, OFF to disable interrupts
 * 
 * Returns:
 *              None
 * ======================================================================== */
void setInterrupts(int toggle) {
    /* Get current status register value */
    unsigned int status = getSTATUS();

    if (toggle == ON) {
        /* Enable interrupts */
        status |= STATUS_IEc;
    } else {
        /* Disable interrupts */
        status &= ~STATUS_IEc;
    }
    /* Set the status register */
    setSTATUS(status);
}

/* Done */
/* ========================================================================
 * Function: resumeState
 *
 * Description: Loads a processor state using the LDST instruction.
 *              This is a wrapper around the LDST macro for cleaner code.
 * 
 * Parameters:
 *              state - Pointer to the processor state to load
 * 
 * Returns:
 *              Does not return (control passes to the loaded state)
 * ======================================================================== */
void resumeState(state_t *state) {
    /* Check if state pointer is NULL */
    if (state == NULL) {
        /* Critical error - state is NULL */
        PANIC();
    }

    /* Load the state into processor */
    LDST(state);
    
    /* Control never returns here */
}

/* Done */
/* ========================================================================
 * Function: clearSwapPoolEntries
 *
 * Description: Clears all swap pool entries for the current process.
 *              This function is called when a process terminates.
 * 
 * Parameters:
 *              None
 * 
 * Returns:
 *              None
 * ======================================================================== */
void clearSwapPoolEntries() {
    /* Get the current process's ASID */
    int asid = getCurrentSupportStruct()->sup_asid;
    if (asid <= 0) {
        /* Invalid ASID, nothing to clear */
        return;
    }

    if (swapPool == NULL) {
        /* Swap pool not initialized */
        return;
    }

    /* Disable interrupts during critical section */
    setInterrupts(OFF);

    /* Acquire the Swap Pool semaphore (SYS3) */
    SYSCALL(PASSEREN, &swapPoolMutex, 0, 0);

    /* Clear all swap pool entries for the current process */
    int i;
    for (i = 0; i < SWAPPOOLSIZE; i++) {
        if (swapPool[i].asid == asid) {
            swapPool[i].valid = FALSE;
        }
    }

    /* Release the Swap Pool semaphore (SYS4) */
    SYSCALL(VERHOGEN, &swapPoolMutex, 0, 0);

    /* Re-enable interrupts */
    setInterrupts(ON);
}