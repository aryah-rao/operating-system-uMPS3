/******************************* sysSupport.c *************************************
 *
 * Module: System Support
 *
 * Description:
 * This module implements the Support Level's exception handlers for
 * non-TLB exceptions that are passed up from the Nucleus.
 * The module handles user-level system calls (SYS9 and above).
 *
 * Policy Decisions:
 * - Memory Protection: All user-provided addresses are validated to ensure they
 *   fall within user address space (KUSEG), preventing access to kernel memory
 * - Error Handling: Invalid parameters or addresses result in immediate process
 *   termination

 * Functions:
 * - genExceptionHandler: Routes exceptions to appropriate handlers based on cause
 * - syscallExceptionHandler: Dispatches user-level SYSCALL requests
 * - programTrapExceptionHandler: Handles program traps passed up to Support Level
 * - getCurrentSupportStruct: Helper to get current process's support structure
 * - getTimeOfDay: Retrieves current time of day for SYS10
 * - writePrinter: Implements printer write operations for SYS11
 * - writeTerminal: Implements terminal write operations for SYS12
 * - readTerminal: Implements terminal read operations for SYS13
 *
 * Written by Aryah Rao & Anish Reddy
 *
 *****************************************************************************/

#include "../h/sysSupport.h"

/*----------------------------------------------------------------------------*/
/* Foward Declarations for External Functions */
/*----------------------------------------------------------------------------*/
/* vmSupport.c */
extern void terminateUProcess(int *mutex);
extern void setInterrupts(int toggle);
extern void resumeState(state_PTR state);
extern int validateUserAddress(memaddr address);
/* deldayDaemon.c */
extern void delaySyscallHandler(support_PTR supportStruct);
/* deviceSupportDMA.c */
extern int diskPutSyscallHandler(support_PTR supportStruct);
extern int diskGetSyscallHandler(support_PTR supportStruct);
extern int flashPutSyscallHandler(support_PTR supportStruct);
extern int flashGetSyscallHandler(support_PTR supportStruct);

/*----------------------------------------------------------------------------*/
/* Helper Function Prototypes */
/*----------------------------------------------------------------------------*/
HIDDEN void programTrapExceptionHandler();
HIDDEN cpu_t getTimeOfDay();
HIDDEN int writePrinter(support_PTR supportStruct);
HIDDEN int writeTerminal(support_PTR supportStruct);
HIDDEN int readTerminal(support_PTR supportStruct);

/*----------------------------------------------------------------------------*/
/* Global Function Implementations */
/*----------------------------------------------------------------------------*/

/******************************************************************************
 *
 * Function: genExceptionHandler
 *
 * Description: This is the Support Level's general exception handler. It is the entry
 *   point for all non-TLB exceptions that the Nucleus has passed up for
 *   handling (i.e., when the process's p_supportStruct is not NULL).
 *   This handler examines the Cause register in the saved exception state
 *   to determine the specific type of exception and then dispatches
 *   control to the appropriate sub-handler (SYSCALL or Program Trap).
 *
 * Parameters:
 *              None
 *
 * Returns:
 *              None
 *
 *****************************************************************************/
void genExceptionHandler() {
    /* Get the current process's support structure */
    support_PTR supportStruct = getCurrentSupportStruct();
    
    if (supportStruct == NULL) {
        terminateUProcess(NULL); /* Nuke it! */
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
 *   a non-NULL support structure. This handler retrieves the SYSCALL number
 *   from register a0 in the saved exception state and performs the
 *   corresponding service.
 *
 * Parameters:
 *   supportStruct - Pointer to the current process's support structure
 *
 * Returns:
 *   None. This function should not return directly. It should update the
 *   return value in the saved exception state (register v0) and then
 *   return control to the user process using LDST.
 *
 *****************************************************************************/
void syscallExceptionHandler(support_PTR supportStruct) {
    /* Access the saved exception state */
    state_t *exceptState = &(supportStruct->sup_exceptState[GENERALEXCEPT]);
        
    /* Increment PC to next instruction */
    exceptState->s_pc += WORDLEN;

    /* Dispatch based on SYSCALL number */
    switch (exceptState->s_a0) {
        case TERMINATE:     /* SYS9: TERMINATE */
            terminateUProcess(NULL);
            break;
            
        case GET_TOD:       /* SYS10: GET TOD */
            exceptState->s_v0 = getTimeOfDay();
            break;
            
        case WRITEPRINTER:  /* SYS11: WRITE TO PRINTER */
            exceptState->s_v0 = writePrinter(supportStruct);
            break;
            
        case WRITETERMINAL: /* SYS12: WRITE TO TERMINAL */
            exceptState->s_v0 = writeTerminal(supportStruct);
            break;
            
        case READTERMINAL:  /* SYS13: READ FROM TERMINAL */
            exceptState->s_v0 = readTerminal(supportStruct);
            break;

        case DISK_PUT:      /* SYS14: Disk Put */
            exceptState->s_v0 = diskPutSyscallHandler(supportStruct);
            break;

        case DISK_GET:      /* SYS15: Disk Get */
            exceptState->s_v0 = diskGetSyscallHandler(supportStruct);
            break;

        case FLASH_PUT:     /* SYS16: Flash Put */
            exceptState->s_v0 = flashPutSyscallHandler(supportStruct);
            break;

        case FLASH_GET:     /* SYS17: Flash Get */
            exceptState->s_v0 = flashGetSyscallHandler(supportStruct);
            break;

        case DELAY:         /* SYS18: DELAY */
            delaySyscallHandler(supportStruct);
            break;
            
        default:            /* Unknown SYSCALL number */
            programTrapExceptionHandler();
            break;
    }

    /* Return to user process */
    resumeState(exceptState);    
}

/******************************************************************************
 *
 * Function: getCurrentSupportStruct
 *
 * Description: Retrieves the current process's support structure using the 
 *               Nucleus' GETSUPPORTPTR syscall.
 *
 * Parameters:
 *              None
 *
 * Returns:
 *              Pointer to the current process's support structure
 *
 *****************************************************************************/
support_PTR getCurrentSupportStruct() {
    return (support_PTR)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
}

/*----------------------------------------------------------------------------*/
/* Helper Function Implementations */
/*----------------------------------------------------------------------------*/

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
 * Returns:
 *   None.
 *
 *****************************************************************************/
void programTrapExceptionHandler() {
    terminateUProcess(NULL); /* Nuke it! */
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
 * Returns:
 *   Current time of day value.
 *
 *****************************************************************************/
cpu_t getTimeOfDay() {
    /* Retrieve current time of day */
    cpu_t currentTime;
    STCK(currentTime);
    return currentTime;
}


/******************************************************************************
 *
 * Function: writePrinter
 *
 * Description: Writes a character string to the printer device associated 
 *              with the process
 *
 * Parameters:
 *              supportStruct - Pointer to the process's support structure
 *
 * Returns:
 *              Number of characters written, or negative of status code
 *
 *****************************************************************************/
int writePrinter(support_PTR supportStruct) {
    char *charAddress = (char*)supportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    int length = supportStruct->sup_exceptState[GENERALEXCEPT].s_a2;
    int printNum = supportStruct->sup_asid-1;

    /* Validate address and length */
    if ((((memaddr)charAddress < KUSEG)) || (length <= 0) || (length > MAXSTRINGLEN)) {
        terminateUProcess(NULL); /* Nuke it! */
    }

    /* Calculate device semaphore index based on line and device number */
    int printerMutex = ((PRNTINT - MAPINT) * DEV_PER_LINE) + (printNum);
        
    /* Gain device mutex for the printer device */
    SYSCALL(PASSEREN, (int)&deviceMutex[printerMutex], 0, 0);

    int index = 0; /* Number of characters written */
    devregarea_t* devRegisterArea = (devregarea_t*) RAMBASEADDR;

    while (index < length) {
        devRegisterArea->devreg[printerMutex].d_data0 = *charAddress; /* Character to write */

        /* Atomically write a character to the printer device */
        setInterrupts(OFF);
        devRegisterArea->devreg[printerMutex].d_command = PRINTCHR;
        int status = SYSCALL(WAITIO, PRNTINT, printNum, 0);
        setInterrupts(ON);

        if (status != READY) { /* Write failed */
            SYSCALL(VERHOGEN, (int)&deviceMutex[printerMutex], 0, 0);
            return -status;
        }
        /* Next character */
        index++;
        charAddress++;
    }

    /* Release device mutex for the printer device */
    SYSCALL(VERHOGEN, (int)&deviceMutex[printerMutex], 0, 0);

    return index; /* Return the number of characters written */
}

/******************************************************************************
 *
 * Function: writeTerminal
 *
 * Description: Writes a character string to the terminal device associated 
 *              with the process
 *
 * Parameters:
 *              supportStruct - Pointer to the process's support structure
 *
 * Returns:
 *              Number of characters written, or negative of status code
 *
 *****************************************************************************/
int writeTerminal(support_PTR supportStruct) {
    char *charAddress = (char*)supportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    int length = supportStruct->sup_exceptState[GENERALEXCEPT].s_a2;
    int termNum = supportStruct->sup_asid-1;

    /* Validate address and length */
    if (((memaddr)charAddress < KUSEG) || (length <= 0) || (length > MAXSTRINGLEN)) {
        terminateUProcess(NULL); /* Nuke it! */
    }

    /* Calculate device semaphore index based on line and device number */
    int termMutex = ((TERMINT - MAPINT) * DEV_PER_LINE) + (termNum);

    /* Gain device mutex for the terminal device */
    SYSCALL(PASSEREN, (int)&deviceMutex[termMutex], 0, 0);

    int index = 0; /* Number of characters written */
    devregarea_t* devRegisterArea = (devregarea_t*) RAMBASEADDR;

    while (index < length) {
        /* Atomically write a character to the terminal device */
        setInterrupts(OFF);
        devRegisterArea->devreg[termMutex].t_transm_command = *charAddress << BYTELEN | PRINTCHR;
        int status = SYSCALL(WAITIO, TERMINT, termNum, 0);
        setInterrupts(ON);

        if ((status & TERMSTATMASK) != RECVD) { /* Write failed */
            SYSCALL(VERHOGEN, (int)&deviceMutex[termMutex], 0, 0);
            return -status;
        }
        /* Next character */
        index++;
        charAddress++;
    }

    /* Release device mutex for the terminal device */
    SYSCALL(VERHOGEN, (int)&deviceMutex[termMutex], 0, 0);

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
 *   supportStruct - Pointer to the process's support structure
 *
 * Returns:
 *   Number of characters read, or negative of status code
 *
 *****************************************************************************/
int readTerminal(support_PTR supportStruct) {
    char *charAddress = (char*)supportStruct->sup_exceptState[GENERALEXCEPT].s_a1;
    int termNum = supportStruct->sup_asid-1;

    /* Validate address */
    if ((memaddr)charAddress < KUSEG) {
        terminateUProcess(NULL); /* Nuke it! */
    }

    /* Calculate device semaphore index based on line and device number */
    int termMutex = ((TERMINT - MAPINT) * DEV_PER_LINE) + (termNum) + DEV_PER_LINE;
        
    /* Gain device mutex for the terminal device */
    SYSCALL(PASSEREN, (int)&deviceMutex[termMutex], 0, 0);

    int index = 0; /* Number of characters read */
    devregarea_t* devRegisterArea = (devregarea_t*) RAMBASEADDR;
    int running = TRUE;

    while (running) {
        /* Atomically read a character from the terminal device */
        setInterrupts(OFF);
        devRegisterArea->devreg[termMutex - DEV_PER_LINE].t_recv_command = PRINTCHR;
        int status = SYSCALL(WAITIO, TERMINT, termNum, 1);
        setInterrupts(ON);

        if ((status & TERMSTATMASK) != RECVD) {  /* Read failed */
            SYSCALL(VERHOGEN, (int)&deviceMutex[termMutex], 0, 0);
            return -status;
        }
        index++; /* Increment the number of characters read */

        /* Extract the character from the status */
        char recvChar = status >> BYTELEN;

        /* Write the character to the user memory */
        *charAddress = recvChar;
        charAddress++;

        /* Check for newline character to terminate reading */
        if (recvChar == NEWLINE) {
            running = FALSE;
        }
    }

    /* Release device mutex for the terminal device */
    SYSCALL(VERHOGEN, (int)&deviceMutex[termMutex], 0, 0);

    return index; /* Return the number of characters read */
}
