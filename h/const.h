#ifndef CONSTS
#define CONSTS

/****************************************************************************
 *
 * This header file contains utility constants & macro definitions.
 *
 * Written by Mikeyg
 *
 * Modified by Aryah Rao and Anish Reddy
 *
 ****************************************************************************/

/* Hardware & software constants */
#define PAGESIZE            4096            /* page size in bytes	*/
#define WORDLEN             4               /* word size in bytes	*/

/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR         0x10000000
#define RAMBASESIZE         0x10000004
#define TODLOADDR           0x1000001C
#define INTERVALTMR         0x10000020
#define TIMESCALEADDR       0x10000024

/* utility constants */
#define TRUE                1
#define FALSE               0
#define HIDDEN              static
#define EOS                 '\0'
#define ERROR               -1
#define SUCCESS             0
#define NULL                ((void *)0xFFFFFFFF)
#define ON                  1
#define OFF                 0

/* device interrupts */
#define PROCINT             0
#define PLTINT              1
#define ITINT               2
#define DISKINT             3
#define FLASHINT            4
#define NETWINT             5
#define PRNTINT             6
#define TERMINT             7

#define DEVINTNUM           5               /* interrupt lines used by devices */
#define DEVPERINT           8               /* devices per interrupt line */
#define DEVREGLEN           4               /* device register field length in bytes, and regs per dev */
#define DEVREGSIZE          16              /* device register size in bytes */

/* device register field number for non-terminal devices */
#define STATUS              0
#define COMMAND             1
#define DATA0               2
#define DATA1               3

/* device register field number for terminal devices */
#define RECVSTATUS          0
#define RECVCOMMAND         1
#define TRANSTATUS          2
#define TRANCOMMAND         3
#define TRANSM_BIT          0x0000000F

/* device common STATUS codes */
#define UNINSTALLED         0
#define READY               1
#define BUSY                3

/* device common COMMAND codes */
#define RESET               0
#define ACK                 1

/* Memory related constants */
#define KUSEG               0x80000000
#define RAMSTART            0x20000000
#define BIOSDATAPAGE        0x0FFFF000
#define PASSUPVECTOR        0x0FFFF900
#define KERNEL_STACK        0x20001000
#define RAMTOP              *(int *)RAMBASEADDR + *(int *)RAMBASESIZE
#define SEG0			    0x00000000
#define SEG1			    0x40000000
#define SEG2			    0x80000000
#define SEG3			    0xC0000000

/* All bits off */
#define ALLOFF              0x0

/* operations */
#define MIN(A, B)           ((A) < (B) ? A : B)
#define MAX(A, B)           ((A) < (B) ? B : A)
#define ALIGNED(A)          (((unsigned)A & 0x3) == 0)

/* Macro to load the Interval Timer */
#define LDIT(T)             ((*((cpu_t *)INTERVALTMR)) = (T) * (*((cpu_t *)TIMESCALEADDR)))

/* Macro to read the TOD clock */
#define STCK(T)             ((T) = ((*((cpu_t *)TODLOADDR)) / (*((cpu_t *)TIMESCALEADDR))))

/* Max number of processes */
#define MAXPROC             20

/* Max integer value */
#define MAXINT              0x7fffffff

/* System Call Mnemonics */
#define CREATEPROCESS       1
#define TERMINATEPROCESS    2
#define PASSEREN            3   
#define VERHOGEN            4
#define WAITIO              5
#define GETCPUTIME          6
#define WAITCLOCK           7
#define GETSUPPORTPTR       8

/* Exception Types */
#define INTERRUPTS          0
#define TLBMOD              1
#define TLBINVLDL           2
#define TLBINVLDS           3
#define ADDRINVLD           4
#define ADDRINVLDS          5
#define BUSINVLD            6
#define BUSINVLDL           7
#define SYSCALLS            8
#define BREAKPOINT          9
#define RESERVEDINST        10
#define COPROCUNUSABLE      11
#define ARITHOVERFLOW       12
#define PGFAULTEXCEPT       0
#define GENERALEXCEPT       1

/* Status Register bits */
#define STATUS_IEp          0x00000004      /* Previous Interrupt Enable */
#define STATUS_VMp          0x02000000      /* Previous Virtual Memory */
#define STATUS_TE           0x08000000      /* Timer Enable */
#define STATUS_KUp          0x00000008      /* Previous Kernel/User mode */
#define STATUS_KUc          0x00000002      /* Current Kernel/User mode */
#define STATUS_IEc          0x00000001      /* Current Interrupt Enable */

/* Cause Register bits */
#define CAUSE_IP_MASK       0x0000FF00      /* Interrupt Pending mask */
#define CAUSE_EXCCODE_MASK  0x0000007C      /* Exception Code mask */
#define CAUSE_EXCCODE_SHIFT 2               /* Exception Code shift for Cause Register */
#define CAUSE_IP_SHIFT      8               /* Interrupt Pending shift for Cause Register */

/* Timer Constants */
#define QUANTUM             5000            /* Time slice quantum in microseconds */
#define CLOCKINTERVAL       100000UL        /* Clock tick interval in microseconds */

/* Device Constants */
#define DEVICE_COUNT        49              /* Total number of devices */
#define DEV_PER_LINE        8
#define MAPINT              3               /* First device interrupt line */

/*Interrupt Mask Constants*/
#define PROCINTERRUPT       0x00000100
#define PLTINTERRUPT        0x00000200
#define ITINTERRUPT         0x00000400
#define DISKINTERRUPT       0x00000800
#define FLASHINTERRUPT      0x00001000
#define NETWORKINTERRUPT    0x00002000
#define PRINTERINTERRUPT    0x00004000
#define TERMINTERRUPT       0x00008000

/* Addresses in RAM */
#define OS_HEADER           ((memaddr *)KERNEL_STACK)   /* 0x20001000 */
#define OS_TEXT_START       (OS_HEADER[2])              /* 0x0008 */ 
#define OS_TEXT_SIZE        (OS_HEADER[3])              /* 0x000C */
#define OS_DATA_START       (OS_HEADER[4])              /* 0x0010 */
#define OS_DATA_SIZE        (OS_HEADER[5])              /* 0x0014 */
#define SWAPPOOLSTART_UNALIGNED (KERNEL_STACK + OS_TEXT_SIZE + OS_DATA_SIZE)   /* Swap pool start address is at the end of the text and data sections */
#define SWAPPOOLSTART           (((SWAPPOOLSTART_UNALIGNED + PAGESIZE - 1) / PAGESIZE) * PAGESIZE)  /* Align to page boundary */
#define FRAMETOADDR(frameNum)   (SWAPPOOLSTART + ((frameNum) * PAGESIZE)) /* Frame to address */
#define UPROC_STACK_BASE(i) (RAMTOP - ((i) * 2 * PAGESIZE))     /* Stack from one page below the top */
#define UPROC_TLB_STACK(i)  (UPROC_STACK_BASE(i) + PAGESIZE)    /* Page fault stack */
#define UPROC_GEN_STACK(i)  (UPROC_STACK_BASE(i))               /* General exception stack */
#define USTACKPAGE          0xC0000000      /* User stack page base address */
#define UTEXTSTART          0x800000B0      /* First page in the text */
#define UPAGESTACK          0xBFFFF000
#define LASTUPROCPAGE       (KUSEG + ((MAXPAGES - 2) * PAGESIZE))   /* Last user process page address */
#define DAEMON_STACK        (UPROC_STACK_BASE(MAXUPROC) - PAGESIZE) /* Daemon stack base address */

/* Hardware Constants */
#define PRINTCHR	        2
#define MAXSTRINGLEN        128
#define BYTELEN	            8
#define RECVD	            5
#define TERMSTATMASK        0x000000FF
#define NEWLINE             0x0A            /* Newline character for terminal and printer output */
#define READ                2               /* BackingStoreRW read command */
#define WRITE               3               /* BackingStoreRW write command */
#define FLASHSHIFT          8               /* Flash device command shift */

/* Virtual Memory Constants */
#define MAXUPROC            8               /* Maximum number of U-procs to create */
#define MAXPAGES            32              /* Maximum number of pages to allocate */
#define SWAPPOOLSIZE        (MAXUPROC * 2)  /* Size of the swap pool */
#define VPNSHIFT            12              /* Virtual Page Number shift */
#define VPNMASK             0xFFFFF000      /* Virtual Page Number mask */
#define VALIDON             0x00000200      /* Valid bit for page table entries */
#define DIRTYON             0x00000400      /* Dirty bit for page table entries */
#define UNOCCUPIED          -1              /* Unoccupied asid */
#define PROBESHIFT          31              /* Shift for probe in index */
#define ASIDSHIFT           6               /* Shift for ASID */
#define USTACKNUM           31              /* Stack page number for user processes */

/* level 1 SYS calls */
#define TERMINATE           9               /* SYSCALL number for TERMINATE (SYS9) */
#define GET_TOD             10              /* SYSCALL number for GET TOD (SYS10) */
#define WRITEPRINTER        11              /* SYSCALL number for WRITE TO PRINTER (SYS11) */
#define WRITETERMINAL       12              /* SYSCALL number for WRITE TO TERMINAL (SYS12) */
#define READTERMINAL        13              /* SYSCALL number for READ FROM TERMINAL (SYS13) */
#define DISK_GET		    14
#define DISK_PUT		    15
#define	FLASH_GET		    16
#define FLASH_PUT		    17
#define DELAY			    18
#define PSEMVIRT		    19
#define VSEMVIRT		    20

/*
 * To allocate a stack for your daemon process below all UProc stack pages:
 *
 * - Each UProc stack is allocated from the top of RAM downward using:
 *     #define UPROC_STACK_BASE(i) (RAMTOP - ((i) * 2 * PAGESIZE))
 *   for i = 1 to MAXUPROC (each UProc gets 2 pages).
 *
 * - The lowest UProc stack base is:
 *     UPROC_STACK_BASE(MAXUPROC)
 *
 * - To allocate a stack for your daemon process just below all UProc stacks:
 *     #define DAEMON_STACK (UPROC_STACK_BASE(MAXUPROC) - PAGESIZE)
 *   This gives the base address for a 1-page stack for the daemon.
 *
 * Example usage:
 *     daemonState.s_sp = DAEMON_STACK + PAGESIZE; // Stack pointer at top of daemon stack
 *
 * This ensures the daemon stack does not overlap with any UProc stack.
 */

/*
 * RAM Layout (as defined by macros and usage in your code):
 *
 * 0x00000000 - 0x1FFFFFFF : Unused (reserved for other segments, not kernel RAM)
 * 0x20000000 - 0x20000FFF : Kernel stack (KERNEL_STACK)
 * 0x20001000 - ...        : OS header, text, and data (OS_HEADER, OS_TEXT_START, OS_TEXT_SIZE, OS_DATA_START, OS_DATA_SIZE)
 * ...                     : Swap pool (SWAPPOOLSTART)
 * ...                     : Free RAM for kernel data structures, static arrays, etc.
 * ...                     : User process stacks (UPROC_STACK_BASE(i), UPROC_TLB_STACK(i), UPROC_GEN_STACK(i))
 * ... up to RAMTOP        : Top of physical RAM (RAMTOP)
 *
 * Key regions:
 * - KERNEL_STACK:      0x20000000 (4KB for kernel stack)
 * - OS_HEADER:         0x20001000 (start of OS header)
 * - OS_TEXT_START:     OS_HEADER[2] (text section start)
 * - OS_TEXT_SIZE:      OS_HEADER[3] (text section size)
 * - OS_DATA_START:     OS_HEADER[4] (data section start)
 * - OS_DATA_SIZE:      OS_HEADER[5] (data section size)
 * - SWAPPOOLSTART:     After OS text and data, page-aligned
 * - UPROC_STACK_BASE(i): RAMTOP - (i * 2 * PAGESIZE) (each U-proc gets 2 pages for stacks, from the top of RAM downward)
 * - UPROC_TLB_STACK(i): UPROC_STACK_BASE(i) + PAGESIZE (TLB exception stack for U-proc i)
 * - UPROC_GEN_STACK(i): UPROC_STACK_BASE(i) (General exception stack for U-proc i)
 * - RAMTOP:            *(int *)RAMBASEADDR + *(int *)RAMBASESIZE (top of physical RAM)
 *
 * The kernel and OS data structures (PCBs, semaphores, etc.) are statically allocated in the BSS/data segment,
 * which is placed after the kernel stack and before the swap pool/user stacks.
 *
 * User process memory (virtual) is mapped in KUSEG (0x80000000+), but their kernel stacks and exception stacks
 * are in physical RAM as described above.
 */

#endif