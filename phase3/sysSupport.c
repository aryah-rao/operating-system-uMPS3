/******************************************************************************
 *
 * Module: System Support
 *
 * Description:
 * This module implements the Support Level's exception handlers for
 * non-TLB exceptions that are passed up from the Nucleus (Level 3).
 * It handles user-level system calls (SYS9 and above) and program traps,
 * providing services such as process termination, time of day retrieval,
 * and I/O operations for user processes.
 *
 * Implementation:
 * The module contains a general exception handler that determines the type of
 * exception and dispatches to appropriate sub-handlers. For system calls, it
 * implements services including process termination, time retrieval, device I/O
 * for printers and terminals. The module ensures proper validation of user
 * addresses and parameters, and manages device operations through mutual exclusion
 * and proper interrupt handling.
 *
 * Functions:
 * - genExceptionHandler: Routes exceptions to appropriate handlers based on cause.
 * - syscallExceptionHandler: Dispatches user-level SYSCALL requests.
 * - programTrapExceptionHandler: Handles program traps passed up to Support Level.
 * - getCurrentSupportStruct: Helper to get current process's support structure.
 * - getTimeOfDay: Retrieves current time of day for SYS10.
 * - writePrinter: Implements printer write operations for SYS11.
 * - writeTerminal: Implements terminal write operations for SYS12.
 * - readTerminal: Implements terminal read operations for SYS13.
 * - terminateUProc: Terminates user process with proper cleanup.
 *
 * Written by Aryah Rao & Anish Reddy
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
HIDDEN unsigned int getTimeOfDay();
HIDDEN int writePrinter(support_PTR supportStruct);
HIDDEN int writeTerminal(support_PTR supportStruct);
HIDDEN int readTerminal(support_PTR supportStruct);


/* Forward declaration for functions in vmSupport.c */
extern void setInterrupts(int onOff);     /* Set interrupts on or off */
extern void resumeState(state_t *state);    /* Load processor state */
extern void terminateUProc(int *mutex);   /* Terminate the current user process */
extern int validateUserAddress(unsigned int address); /* Validate user address */

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
 *****************************************************************************/
void genExceptionHandler() {
    /* Get the current process's Support Structure */
    support_PTR supportStruct = getCurrentSupportStruct();
    
    if (supportStruct == NULL) {
        /* This shouldn't happen; terminate if it does */
        terminateUProc(NULL);
        return;
    }
    
    /* Access the saved exception state */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
    
    /* Extract exception code from Cause register */
    unsigned int cause = (exceptState->s_cause & CAUSE_EXCCODE_MASK) >> CAUSE_EXCCODE_SHIFT;
    
    if (cause == SYSCALLS) {
        /* SYSCALL exception */
        syscallExceptionHandler(supportStruct);
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
 *   supportStruct - Pointer to the current process's Support Structure
 *
 * Return Value:
 *   None. This function should not return directly. It should update the
 *   return value in the saved exception state (register v0) and then
 *   return control to the user process using LDST.
 *
 *****************************************************************************/
void syscallExceptionHandler(support_PTR supportStruct) {
    /* Access the saved exception state */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
    
    /* Retrieve the SYSCALL number from a0 */
    int sysNum = exceptState->s_a0;
    
    /* Dispatch based on SYSCALL number */
    switch (sysNum) {
        case TERMINATE:
            /* SYS9: TERMINATE */
            terminateUProc(NULL);
            break;
            
        case GETTOD:
            /* SYS10: GET TOD */
            exceptState->s_v0 = getTimeOfDay();
            break;
            
        case WRITEPRINTER:
            /* SYS11: WRITE TO PRINTER */
            {
                exceptState->s_v0 = writePrinter(supportStruct);
            }
            break;
            
        case WRITETERMINAL:
            /* SYS12: WRITE TO TERMINAL */
            {
                    exceptState->s_v0 = writeTerminal(supportStruct);
            }
            break;
            
        case READTERMINAL:
            /* SYS13: READ FROM TERMINAL */
            {
                exceptState->s_v0 = readTerminal(supportStruct);
            }
            break;
            
        default:
            /* Unknown SYSCALL number */
            terminateUProc(NULL);
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
 *   None.
 *
 * Return Value:
 *   None.
 *
 *****************************************************************************/
extern void programTrapExceptionHandler() {
    /* Simply terminate the process - semaphore release is handled in terminateUProc() */
    terminateUProc(NULL);
}

/*----------------------------------------------------------------------------*/
/* Helper Functions (Implementation) */
/*----------------------------------------------------------------------------*/

/******************************************************************************
 *
 * Function: getCurrentSupportStruct
 *
 * Description:
 *   Helper function to retrieve the Support Structure pointer for the current
 *   process through a system call to the nucleus.
 *
 * Parameters:
 *   None.
 *
 * Return Value:
 *   Pointer to the current process's Support Structure, or NULL if none.
 *
 *****************************************************************************/
extern support_PTR getCurrentSupportStruct() {
    return (support_PTR)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
}

/******************************************************************************
 *
 * Function: getTimeOfDay
 *
 * Description:
 *   Retrieves the current time of day using the nucleus GETCPUTIME syscall.
 *   This implements the functionality for SYS10.
 *
 * Parameters:
 *   None.
 *
 * Return Value:
 *   Current time of day value.
 *
 *****************************************************************************/
HIDDEN unsigned int getTimeOfDay() {
    /* Use the Nucleus's GETCPUTIME syscall */
    return SYSCALL(GETCPUTIME, 0, 0, 0);
}

/******************************************************************************
 *
 * Function: writePrinter
 *
 * Description:
 *   Writes a character string to the printer device associated with the process.
 *   This implements the functionality for SYS11.
 *
 * Parameters:
 *   supportStruct - Pointer to the process's Support Structure
 *
 * Return Value:
 *   Number of characters written, or ERROR if the operation failed.
 *
 *****************************************************************************/
HIDDEN int writePrinter(support_PTR supportStruct) {
    char *firstChar = (char*)supportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    int length = supportStruct->sup_exceptState[GENERALEXCEPT].s_a2;
    int printNum = supportStruct->sup_asid-1;

    /* Validate address and length */
    if (!validateUserAddress((unsigned int)firstChar) || length <= 0 || length > MAXSTRINGLEN) {
        /* Invalid address or length, terminate the process */
        terminateUProc(NULL);
        return ERROR;
    }
    
    /* Access device registers from RAMBASE address */
    devregarea_t* devRegisterArea = (devregarea_t*) RAMBASEADDR;

    /* Calculate device semaphore index based on line and device number */
    int devSemaphore = ((PRNTINT - MAPINT) * DEV_PER_LINE) + (printNum);
        
    /* Gain device mutex for the printer device */
    SYSCALL(PASSEREN, (unsigned int)&deviceMutex[devSemaphore], 0, 0);

    int index = 0; /* Number of characters written */

    while (index < length) {
        /* Atomically write a character to the printer device */
        setInterrupts(OFF); /* Disable interrupts to ensure atomicity */
        devRegisterArea->devreg[devSemaphore].d_command = *firstChar; /* Character to write */
        devRegisterArea->devreg[devSemaphore].d_command = PRINT;
        int status = SYSCALL(WAITIO, PRNTINT, printNum, 0);
        setInterrupts(ON); /* Re-enable interrupts */

        if (status != READY) {
            return -status; /* Return error if write fails */
        }
        index++;
        firstChar++;
    }

    /* Release device mutex for the printer device */
    SYSCALL(VERHOGEN, (unsigned int)&deviceMutex[devSemaphore], 0, 0);

    return index; /* Return the number of characters written */
}

/******************************************************************************
 *
 * Function: writeTerminal
 *
 * Description:
 *   Writes a character string to the terminal device associated with the process.
 *   This implements the functionality for SYS12.
 *
 * Parameters:
 *   supportStruct - Pointer to the process's Support Structure
 *
 * Return Value:
 *   Number of characters written, or ERROR if the operation failed.
 *
 *****************************************************************************/
HIDDEN int writeTerminal(support_PTR supportStruct) {
    char *firstChar = (char*)supportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    int length = supportStruct->sup_exceptState[GENERALEXCEPT].s_a2;
    int termNum = supportStruct->sup_asid-1;

    /* Validate address and length */
    if (!validateUserAddress((unsigned int)firstChar) || length <= 0 || length > MAXSTRINGLEN) {
        /* Invalid address or length, terminate the process */
        terminateUProc(NULL);
        return ERROR;
    }

    /* Access device registers from RAMBASE address */
    devregarea_t* devRegisterArea = (devregarea_t*) RAMBASEADDR;

    /* Calculate device semaphore index based on line and device number */
    int devSemaphore = ((TERMINT - MAPINT) * DEV_PER_LINE) + (termNum);
        
    /* Gain device mutex for the terminal device */
    SYSCALL(PASSEREN, (unsigned int)&deviceMutex[devSemaphore], 0, 0);

    int index = 0; /* Number of characters written */

    while (index < length) {
        /* Atomically write a character to the terminal device */
        setInterrupts(OFF); /* Disable interrupts to ensure atomicity */
        devRegisterArea->devreg[devSemaphore].t_transm_command = *firstChar << BYTELEN | PRINT; /* Character to write */
        int status = SYSCALL(WAITIO, TERMINT, termNum, 0);
        setInterrupts(ON); /* Re-enable interrupts */

        if (status != TRANSCHAR) {
            return -status; /* Return error if write fails */
        }
        index++;
        firstChar++;
    }

    /* Release device mutex for the terminal device */
    SYSCALL(VERHOGEN, (unsigned int)&deviceMutex[devSemaphore], 0, 0);

    return index; /* Return the number of characters written */
}

/******************************************************************************
 *
 * Function: readTerminal
 *
 * Description:
 *   Reads a line of input from the terminal device associated with the process.
 *   Reading continues until a newline character is encountered or an error occurs.
 *   This implements the functionality for SYS13.
 *
 * Parameters:
 *   supportStruct - Pointer to the process's Support Structure
 *
 * Return Value:
 *   Number of characters read, or ERROR if the operation failed.
 *
 *****************************************************************************/
HIDDEN int readTerminal(support_PTR supportStruct) {
    char *firstChar = (char*)supportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    int termNum = supportStruct->sup_asid-1;

    /* Validate address and length */
    if (!validateUserAddress((unsigned int)firstChar)) {
        /* Invalid address or length, terminate the process */
        terminateUProc(NULL);
        return ERROR;
    }

    /* Access device registers from RAMBASE address */
    devregarea_t* devRegisterArea = (devregarea_t*) RAMBASEADDR;

    /* Calculate device semaphore index based on line and device number */
    int devSemaphore = ((TERMINT - MAPINT) * DEV_PER_LINE) + (termNum);
        
    /* Gain device mutex for the terminal device */
    SYSCALL(PASSEREN, (unsigned int)&deviceMutex[devSemaphore], 0, 0);

    int index = 0; /* Number of characters read */
    int running = TRUE;

    while (running) {
        /* Atomically read a character from the terminal device */
        setInterrupts(OFF); /* Disable interrupts to ensure atomicity */
        devRegisterArea->devreg[devSemaphore].t_recv_command = 2; /* Send receive command */
        int status = SYSCALL(WAITIO, TERMINT, termNum, 1);
        setInterrupts(ON); /* Re-enable interrupts */

        if ((status & TERMMASK) != RECVCHAR) {
            return -status; /* Return error if read fails */
        }
        index++; /* Increment the number of characters read */

        /* Extract the character from the status */
        char recvChar = status >> BYTELEN;

        /* Write the character to the user memory */
        *firstChar = recvChar;
        firstChar++;

        /* Check for newline character to terminate reading */
        if (recvChar == NEWLINE) {
            running = FALSE;
        }
    }

    /* Release device mutex for the terminal device */
    SYSCALL(VERHOGEN, (unsigned int)&deviceMutex[devSemaphore], 0, 0);

    return index; /* Return the number of characters read */
}