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
#define PAGESIZE		    4096		/* page size in bytes	*/
#define WORDLEN			    4	        /* word size in bytes	*/


/* timer, timescale, TOD-LO and other bus regs */
#define RAMBASEADDR		    0x10000000
#define RAMBASESIZE		    0x10000004
#define TODLOADDR		    0x1000001C
#define INTERVALTMR		    0x10000020	
#define TIMESCALEADDR	    0x10000024


/* utility constants */
#define	TRUE			    1
#define	FALSE			    0
#define HIDDEN			    static
#define EOS				    '\0'

#define NULL 			    ((void *)0xFFFFFFFF)

/* device interrupts */
#define DISKINT			    3
#define FLASHINT 		    4
#define NETWINT 		    5
#define PRNTINT 		    6
#define TERMINT			    7

#define DEVINTNUM		    5		    /* interrupt lines used by devices */
#define DEVPERINT		    8		    /* devices per interrupt line */
#define DEVREGLEN		    4		    /* device register field length in bytes, and regs per dev */	
#define DEVREGSIZE	        16 		    /* device register size in bytes */

/* device register field number for non-terminal devices */
#define STATUS			    0
#define COMMAND			    1
#define DATA0			    2
#define DATA1			    3

/* device register field number for terminal devices */
#define RECVSTATUS  	    0
#define RECVCOMMAND 	    1
#define TRANSTATUS  	    2
#define TRANCOMMAND 	    3

/* device common STATUS codes */
#define UNINSTALLED		    0
#define READY			    1
#define BUSY			    3

/* device common COMMAND codes */
#define RESET			    0
#define ACK				    1

/* Memory related constants */
#define KSEG0               0x00000000
#define KSEG1               0x20000000
#define KSEG2               0x40000000
#define KUSEG               0x80000000
#define RAMSTART            0x20000000
#define BIOSDATAPAGE        0x0FFFF000
#define	PASSUPVECTOR	    0x0FFFF900
#define RAMTOP              *(int*)RAMBASEADDR + *(int*)RAMBASESIZE

/* Exceptions related constants */
#define	PGFAULTEXCEPT	    0
#define GENERALEXCEPT	    1

/* Processor status bits for status register */
#define ALLOFF              0x0  /* All bits off */
#define IECON               0x00000001  /* Interrupt Enable ON */
#define IMON                0x0000FF00  /* Interrupt Mask ON */
#define TEBITON             0x08000000  /* Translation Enable bit */
#define KUP                 0x00000008  /* Kernel/User Previous Mode bit */
#define KUON                0x00000010  /* Kernel/User Current Mode ON (0:kernel, 1:user) */

/* operations */
#define	MIN(A,B)		    ((A) < (B) ? A : B)
#define MAX(A,B)		    ((A) < (B) ? B : A)
#define	ALIGNED(A)		    (((unsigned)A & 0x3) == 0)

/* Macro to load the Interval Timer */
#define LDIT(T)	            ((* ((cpu_t *) INTERVALTMR)) = (T) * (* ((cpu_t *) TIMESCALEADDR))) 

/* Macro to read the TOD clock */
#define STCK(T)             ((T) = ((* ((cpu_t *) TODLOADDR)) / (* ((cpu_t *) TIMESCALEADDR))))

/* Max number of processes */
#define MAXPROC             20

/* Max integer value */
# define MAXINT             0x7fffffff

/* Number of priority levels */
#define NUM_QUEUES          4

/* Kernet Stach Memory Address */
#define KERNEL_STACK        0x20001000

/* System Call Mnemonics */
#define CREATEPROCESS       1
#define TERMINATEPROCESS    2
#define PASSEREN            3
#define VERHOGEN            4
#define WAITIO              5
#define GETCPUTIME          6
#define WAITCLOCK           7
#define GETSUPPORTPTR       8

/* CPU Exception related constants */
#define INT_OLDAREA        0x20000000
#define INT_NEWAREA        0x20000020
#define TLB_OLDAREA        0x20000040
#define TLB_NEWAREA        0x20000060
#define PGMTRAP_OLDAREA    0x20000080
#define PGMTRAP_NEWAREA    0x200000A0
#define SYSBK_OLDAREA      0x200000C0
#define SYSBK_NEWAREA      0x200000E0

/* Exception Types */
#define INTERRUPTS        0
#define TLBMOD            1
#define TLBINVLD          2
#define TLBINVLDL         3
#define ADDRINVLD         4
#define ADDRINVLDS        5
#define BUSINVLD          6
#define BUSINVLDL         7
#define SYSCALL_EXCEPTION 8
#define BREAKPOINT        9
#define RESERVEDINST      10
#define COPROCUNUSABLE    11
#define ARITHOVERFLOW     12

/* Status Register bits */
#define STATUS_IEp        0x00000004  /* Previous Interrupt Enable */
#define STATUS_VMp        0x02000000  /* Previous Virtual Memory */
#define STATUS_TE         0x08000000  /* Timer Enable */
#define STATUS_KUp        0x00000008  /* Previous Kernel/User mode */
#define STATUS_KUc        0x00000002  /* Current Kernel/User mode */
#define STATUS_IE         0x00000001  /* Current Interrupt Enable */

/* Cause Register bits */
#define CAUSE_BD          0x80000000  /* Branch Delay slot */
#define CAUSE_IP_MASK     0x0000FF00  /* Interrupt Pending mask */
#define CAUSE_IP(n)       ((n) << 8)  /* Interrupt Pending bit n */
#define CAUSE_EXCCODE_MASK  0x0000007C
#define CAUSE_EXCCODE_SHIFT 2
#define CAUSE_IP_SHIFT      8

/* Timer Constants */
#define QUANTUM           5000        /* Time slice quantum in microseconds */
#define CLOCKINTERVAL    100000UL      /* Clock tick interval in microseconds */

/* Device Constants */
#define DEVICE_COUNT     48             /* Total number of devices */
#define DEV_PER_INT      8           /* Devices per interrupt line */
#define DEV_TERM_START   0x10000250  /* Terminal device starting address */
#define DEV_DISK_START   0x10000150  /* Disk device starting address */
#define DEV_FLASH_START  0x10000180  /* Flash device starting address */
#define DEV_NETW_START   0x100001B0  /* Network device starting address */
#define DEV_PRNT_START   0x100001E0  /* Printer device starting address */

/* Terminal Status/Command bits */
#define TERM_STATUS_MASK 0x000000FF
#define TERM_READ_STATUS 0x0000000F
#define TERM_WRITE_STATUS 0x000000F0
#define TERM_RX_READY    1
#define TERM_TX_READY    1

/* Interrupt Line Constants */
#define PROCESSOR_INT      0    /* Processor Interrupt line */
#define PLT_INT            1    /* Clock Interrupt line */
#define INTERVAL_INT       2    /* Timer Interrupt line */
#define DISK_INT           3    /* Disk Interrupt line */
#define FLASH_INT          4    /* Flash Interrupt line */
#define NETWORK_INT        5    /* Network Interrupt line */
#define PRINTER_INT        6    /* Printer Interrupt line */
#define TERMINAL_INT       7    /* Terminal Interrupt line */

/* Device Register Access Macros */
#define DEV_REG_ADDR(deviceNum) (DEV_REG_START + ((deviceNum) * DEV_REG_SIZE))
#define INT_DEVICENUMBER(semNum) ((semNum) % DEV_PER_INT)

/* Terminal Device Semaphore Indices */
#define TERM_RX_SEM(dev)   (((TERMINAL_INT - 3) * DEV_PER_INT) + (dev))
#define TERM_TX_SEM(dev)   (TERM_RX_SEM(dev) + DEV_PER_INT)

/* Terminal Status Bits */
#define DEV_TERM_STATUS    0x0F
#define DEV_TERM_READY     1

/* Device Register Base Addresses */
#define DEV_REG_START      0x10000050
#define DEV_REG_SIZE       16

#endif
