#include "const.h"
#include "types.h"
#include "libumps.h"
#include "syscall.h"
#include "vmSupport.h"

// Global variables for Support Level
int deviceSemaphores[DEV_PER_INT * DEV_USED_INTS];
int masterSemaphore = 0;

// Swap Pool semaphore
int swapPoolSemaphore = 1;

// Test function (InstantiatorProcess)
void test()
{
    // Initialize device semaphores
    for (int i = 0; i < DEV_PER_INT * DEV_USED_INTS; i++)
    {
        deviceSemaphores[i] = 1;
    }

    // Initialize U-procs
    for (int i = 1; i <= MAXUPROC; i++)
    {
        state_t uProcState;
        support_t uProcSupport;

        // Initialize processor state for U-proc
        STST(&uProcState);                                             // Copy current state
        uProcState.pc_epc = uProcState.reg_t9 = 0x800000B0;            // Start of .text section
        uProcState.reg_sp = 0xC0000000;                                // Stack pointer
        uProcState.status = STATUS_USER_MODE | STATUS_IEp | STATUS_IM; // User mode, interrupts enabled
        uProcState.entry_hi = i << ASID_SHIFT;                         // Unique ASID

        // Initialize support structure for U-proc
        uProcSupport.sup_asid = i;
        uProcSupport.sup_exceptContext[GENERALEXCEPT].pc = (memaddr)genExceptionHandler;
        uProcSupport.sup_exceptContext[GENERALEXCEPT].status = STATUS_KERNEL_MODE | STATUS_IEp | STATUS_IM;
        uProcSupport.sup_exceptContext[GENERALEXCEPT].reg_sp = (memaddr)&uProcSupport.stack[SUPPORT_STACK_SIZE - 1];
        uProcSupport.sup_exceptContext[PGFAULTEXCEPT].pc = (memaddr)tlbExceptionHandler;
        uProcSupport.sup_exceptContext[PGFAULTEXCEPT].status = STATUS_KERNEL_MODE | STATUS_IEp | STATUS_IM;
        uProcSupport.sup_exceptContext[PGFAULTEXCEPT].reg_sp = (memaddr)&uProcSupport.stack[SUPPORT_STACK_SIZE - 1];
        initializePageTable(&uProcSupport.sup_privatePgTbl);

        // Create U-proc using SYS1
        SYSCALL(SYS1, (memaddr)&uProcState, (memaddr)&uProcSupport, 0);
    }

    // Wait for all U-procs to terminate
    for (int i = 0; i < MAXUPROC; i++)
    {
        SYSCALL(SYS3, (memaddr)&masterSemaphore, 0, 0);
    }

    // Terminate the test process
    SYSCALL(SYS2, 0, 0, 0);
}
