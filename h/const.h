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
#define PAGESIZE		        4096		/* page size in bytes	*/
#define WORDLEN			        4	        /* word size in bytes	*/

/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR		        0x10000000
#define RAMBASESIZE		        0x10000004
#define TODLOADDR		        0x1000001C
#define INTERVALTMR		        0x10000020	
#define TIMESCALEADDR	        0x10000024

/* utility constants */
#define	TRUE			        1
#define	FALSE			        0
#define HIDDEN			        static
#define EOS				        '\0'
#define ERROR			        -1
#define SUCCESS			        0
#define NULL 			        ((void *)0xFFFFFFFF)
#define ON                      1
#define OFF                     0

/* device interrupts */
#define PROCINT			        0
#define PLTINT			        1
#define ITINT			        2
#define DISKINT			        3
#define FLASHINT 		        4
#define NETWINT 		        5
#define PRNTINT 		        6
#define TERMINT			        7

#define DEVINTNUM		        5		    /* interrupt lines used by devices */
#define DEVPERINT		        8		    /* devices per interrupt line */
#define DEVREGLEN		        4		    /* device register field length in bytes, and regs per dev */	
#define DEVREGSIZE	            16 		    /* device register size in bytes */

/* device register field number for non-terminal devices */
#define STATUS			        0
#define COMMAND			        1
#define DATA0			        2
#define DATA1			        3

/* device register field number for terminal devices */
#define RECVSTATUS  	        0
#define RECVCOMMAND 	        1
#define TRANSTATUS  	        2
#define TRANCOMMAND 	        3
#define TRANSM_BIT              0x0000000F

/* device common STATUS codes */
#define UNINSTALLED		        0
#define READY			        1
#define BUSY			        3

/* device common COMMAND codes */
#define RESET			        0
#define ACK				        1

/* Memory related constants */
#define KSEG0                   0x00000000
#define KSEG1                   0x20000000
#define KSEG2                   0x40000000
#define KUSEG                   0x80000000
#define RAMSTART                0x20000000
#define BIOSDATAPAGE            0x0FFFF000
#define	PASSUPVECTOR	        0x0FFFF900
#define KERNEL_STACK            0x20001000
#define RAMTOP                  *(int*)RAMBASEADDR + *(int*)RAMBASESIZE

/* All bits off */
#define ALLOFF                  0x0  

/* operations */
#define	MIN(A,B)		        ((A) < (B) ? A : B)
#define MAX(A,B)		        ((A) < (B) ? B : A)
#define	ALIGNED(A)		        (((unsigned)A & 0x3) == 0)

/* Macro to load the Interval Timer */
#define LDIT(T)	                ((* ((cpu_t *) INTERVALTMR)) = (T) * (* ((cpu_t *) TIMESCALEADDR))) 

/* Macro to read the TOD clock */
#define STCK(T)                 ((T) = ((* ((cpu_t *) TODLOADDR)) / (* ((cpu_t *) TIMESCALEADDR))))

/* Max number of processes */
#define MAXPROC                 20

/* Max integer value */
# define MAXINT                 0x7fffffff

/* System Call Mnemonics */
#define CREATEPROCESS           1
#define TERMINATEPROCESS        2
#define PASSEREN                3
#define VERHOGEN                4
#define WAITIO                  5
#define GETCPUTIME              6
#define WAITCLOCK               7
#define GETSUPPORTPTR           8

/* Exception Types */
#define INTERRUPTS              0
#define TLBMOD                  1           /* TLB Modification */
#define TLBINVLDL               2           /* TLB Invalid Load */
#define TLBINVLDS               3           /* TLB Invalid Store */
#define ADDRINVLD               4
#define ADDRINVLDS              5
#define BUSINVLD                6
#define BUSINVLDL               7
#define SYSCALLS                8
#define BREAKPOINT              9
#define RESERVEDINST            10
#define COPROCUNUSABLE          11
#define ARITHOVERFLOW           12
#define	PGFAULTEXCEPT	        0
#define GENERALEXCEPT	        1

/* Status Register bits */
#define STATUS_IEp              0x00000004  /* Previous Interrupt Enable */
#define STATUS_VMp              0x02000000  /* Previous Virtual Memory */
#define STATUS_TE               0x08000000  /* Timer Enable */
#define STATUS_KUp              0x00000008  /* Previous Kernel/User mode */
#define STATUS_KUc              0x00000002  /* Current Kernel/User mode */
#define STATUS_IEc              0x00000001  /* Current Interrupt Enable */

/* Cause Register bits */
#define CAUSE_IP_MASK           0x0000FF00  /* Interrupt Pending mask */
#define CAUSE_EXCCODE_MASK      0x0000007C  /* Exception Code mask */
#define CAUSE_EXCCODE_SHIFT     2           /* Exception Code shift for Cause Register */
#define CAUSE_IP_SHIFT          8           /* Interrupt Pending shift for Cause Register */

/* Timer Constants */
#define QUANTUM                 5000        /* Time slice quantum in microseconds */
#define CLOCKINTERVAL           100000UL    /* Clock tick interval in microseconds */

/* Device Constants */
#define DEVICE_COUNT            49          /* Total number of devices */
#define DEV_PER_LINE            8
#define MAPINT                  3           /* First device interrupt line */ 

/*Interrupt Mask Constants*/
#define PROCINTERRUPT 	        0x00000100
#define PLTINTERRUPT 	        0x00000200
#define ITINTERRUPT 	        0x00000400
#define DISKINTERRUPT 	        0x00000800
#define FLASHINTERRUPT 	        0x00001000
#define NETWORKINTERRUPT	    0x00002000
#define PRINTERINTERRUPT 	    0x00004000
#define TERMINTERRUPT 	        0x00008000



/* Aryah's additions for Phase 3 */
#define MAXUPROC                8           /* Maximum number of U-procs to create */ 
#define MAXPAGES                32          /* Maximum number of pages to allocate */
#define SWAPPOOLSIZE            MAXUPROC*2  /* Size of the swap pool (2 times the number of U-procs */
#define VPNSHIFT                12          /* Virtual Page Number shift for PTEs */
#define VALIDON                 0x00000200  /* Valid bit for page table entries */
#define DIRTYON                 0x00000400  /* Dirty bit for page table entries */
#define TERMINATE               9           /* SYSCALL number for TERMINATE (SYS9) */
#define GETTOD                  10          /* SYSCALL number for GET TOD (SYS10) */
#define WRITEPRINTER            11          /* SYSCALL number for WRITE TO PRINTER (SYS11) */
#define WRITETERMINAL           12          /* SYSCALL number for WRITE TO TERMINAL (SYS12) */
#define READTERMINAL            13          /* SYSCALL number for READ FROM TERMINAL (SYS13) */
#define USTACKPAGE              0xC0000000  /* User stack page base address (for U-proc) */
#define TEXTSTART               0x800000B0  /* First page in the text section for U-proc initialization */
#define ASIDSHIFT               6          /* Shift for ASID in the PTE entryHI */
#define SWAPSTART               0x20020000 /* Start address for swap space in flash memory (for SYS17) */
#define MAXSTRINGLEN            128         /* Maximum string length for terminal I/O operations */
#define PRINT                   2           
/* Macros for translating between addresses and page/frame numbers */
#define FRAME_TO_ADDR(frame) (SWAPSTART + ((frame) * PAGESIZE))
#define ADDR_TO_FRAME(addr) (((addr) - RAMSTART) / PAGESIZE)


#endif
