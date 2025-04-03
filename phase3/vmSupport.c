/*
 * vmSupport.c - Implementation of the Virtual Memory Support module (Level 4 / Phase 3).
 *
 * This module is responsible for providing virtual memory capabilities in the Pandos
 * operating system. It includes the Translation Lookaside Buffer (TLB) exception
 * handler (the Pager), initial functions for interacting with flash devices for
 * backing store, and management of the Swap Pool.
 *
 * Authors: Aryah Rao and Anish Reddy
 * Date: Current Date
 */

#include "../h/vmSupport.h" // To access the support structure and page table
#include "../h/const.h"
#include "../h/types.h"
#include "/usr/include/umps3/umps/libumps.h"

/* External variables */
extern int swapPoolSema4;

/* Module-wide private variables */
HIDDEN struct {
    /* Implementation of the Swap Pool table */
    struct swap_pool_entry {
        int asid;                  /* Process ID (ASID) */
        int vpn;                   /* Virtual Page Number */
        int flash_dev;             /* Flash device number */
        int block_num;             /* Block number on flash device */
        int valid;                 /* Entry validity flag */
        int frame;                 /* Physical frame number if in memory */
        pageTableEntry_PTR pte;    /* Pointer to page table entry */
    } swapTable[MAXPAGES];         /* One entry per possible page */
    
    int nextVictim;                /* Index for FIFO replacement */
} swapPoolData;

/* Simplify by removing the frame pool structure - we'll use a different approach */

/* Macros for translating between addresses and page/frame numbers */
#define FRAME_TO_ADDR(frame) (RAMSTART + ((frame) * PAGESIZE))
#define ADDR_TO_FRAME(addr) (((addr) - RAMSTART) / PAGESIZE)

/* Public function to initialize the Swap Pool table and semaphore. */
void initSwapStructs() {
    int i;
    for (i = 0; i < MAXPAGES; i++) {
        swapPoolData.swapTable[i].asid = 0;
        swapPoolData.swapTable[i].vpn = 0;
        swapPoolData.swapTable[i].flash_dev = 0;
        swapPoolData.swapTable[i].block_num = 0;
        swapPoolData.swapTable[i].valid = FALSE;
    }
    
    /* Initialize the FIFO replacement pointer */
    swapPoolData.nextVictim = 0;

    /* Initialize the Swap Pool semaphore */
    swapPoolSema4 = 1;
}

/* HIDDEN helper function to allocate a free Swap Pool entry. */
HIDDEN int allocateSwapEntry(int asid, int vpn, int *flashDev, int *blockNum) {
    /*
     * Acquire the Swap Pool semaphore (SYS3).
     */
    SYSCALL(PASSEREN, &swapPoolSema4, 0, 0);

    /*
     * Search the swapPoolTable for a free entry.
     * A free entry might be one that is marked as invalid or belongs to a terminated process.
     * If a suitable entry is found, mark it as used by the current process (ASID)
     * and virtual page number (VPN), and set the flash device and block number.
     * Return 0 on success and -1 if no free entry is found.
     */
    int i;
    for (i = 0; i < MAXPAGES; i++) {
        if (!swapPoolData.swapTable[i].valid) {
            /* Found a free entry */
            swapPoolData.swapTable[i].asid = asid;
            swapPoolData.swapTable[i].vpn = vpn;
            swapPoolData.swapTable[i].flash_dev = asid % 8; /* Assign flash device based on ASID */
            swapPoolData.swapTable[i].block_num = i;        /* Use entry index as block number */
            swapPoolData.swapTable[i].valid = TRUE;
            
            /* Return the assigned flash device and block number */
            *flashDev = swapPoolData.swapTable[i].flash_dev;
            *blockNum = swapPoolData.swapTable[i].block_num;
            
            /*
             * Release the Swap Pool semaphore (SYS4).
             */
            SYSCALL(VERHOGEN, &swapPoolSema4, 0, 0);
            return SUCCESS;
        }
    }

    /*
     * Release the Swap Pool semaphore (SYS4).
     */
    SYSCALL(VERHOGEN, &swapPoolSema4, 0, 0);
    return ERROR; /* No free entry found */
}

/* HIDDEN helper function to deallocate a Swap Pool entry. */
HIDDEN void deallocateSwapEntry(int asid, int vpn) {
    /*
     * Acquire the Swap Pool semaphore (SYS3).
     */
    SYSCALL(PASSEREN, &swapPoolSema4, 0, 0);

    /*
     * Search the swapPoolTable for the entry corresponding to the given ASID and VPN.
     * Mark this entry as free (e.g., set the valid bit to 0).
     */
    int i;
    for (i = 0; i < MAXPAGES; i++) {
        if (swapPoolData.swapTable[i].valid && 
            swapPoolData.swapTable[i].asid == asid && 
            swapPoolData.swapTable[i].vpn == vpn) {
            /* Found the entry - mark it invalid */
            swapPoolData.swapTable[i].valid = FALSE;
            break;
        }
    }

    /*
     * Release the Swap Pool semaphore (SYS4).
     */
    SYSCALL(VERHOGEN, &swapPoolSema4, 0, 0);
}

/* HIDDEN function to find a swap entry for a given ASID and VPN */
HIDDEN int findSwapEntry(int asid, int vpn, int *flashDev, int *blockNum) {
    int i;
    for (i = 0; i < MAXPAGES; i++) {
        if (swapPoolData.swapTable[i].valid && 
            swapPoolData.swapTable[i].asid == asid && 
            swapPoolData.swapTable[i].vpn == vpn) {
            /* Found the entry */
            *flashDev = swapPoolData.swapTable[i].flash_dev;
            *blockNum = swapPoolData.swapTable[i].block_num;
            return SUCCESS;
        }
    }
    return ERROR; /* Entry not found */
}

/* HIDDEN function to allocate a physical frame - simplified FIFO approach */
HIDDEN int allocateFrame(int *frameNum) {
    /* Simply use the nextVictim counter directly */
    *frameNum = swapPoolData.nextVictim;
    
    /* Update the FIFO index for next time */
    swapPoolData.nextVictim = (swapPoolData.nextVictim + 1) % MAXPAGES;
    
    return SUCCESS;
}

/* HIDDEN function to read a page from the flash device. */
HIDDEN int readFlashPage(int flashDev, int blockNum, memaddr physicalAddress) {
    /*
     * Initiate a synchronous read operation on the specified flash device and block number.
     * This will involve using the SYS17 (FLASH GET) system call.
     * The data read from the flash device should be placed at the provided physicalAddress in RAM.
     * Return the completion status of the operation.
     * Handle potential errors (e.g., invalid flash device or block number) - though
     * such checks might be done by the caller (Pager).
     */
    return SYSCALL(DISK_GET, (unsigned int *)physicalAddress, flashDev, blockNum);
}

/* HIDDEN function to write a page to the flash device. */
HIDDEN int writeFlashPage(int flashDev, int blockNum, memaddr physicalAddress) {
    /*
     * Initiate a synchronous write operation on the specified flash device and block number.
     * This will involve using the SYS16 (FLASH PUT) system call.
     * The data at the provided physicalAddress in RAM should be written to the flash device.
     * Return the completion status of the operation.
     * Handle potential errors (e.g., invalid flash device or block number) - though
     * such checks might be done by the caller (Pager).
     */
    return SYSCALL(DISK_PUT, (unsigned int *)physicalAddress, flashDev, blockNum);
}

/* TLB exception handler (The Pager). */
void uTLB_RefillHandler() {
    /*
     * 1. Determine the virtual address that caused the TLB miss.
     */
    unsigned int badVAddr = getBADVADDR();

    /*
     * 2. Extract the Virtual Page Number (VPN) from the BadVAddr.
     */
    unsigned int vpn = badVAddr >> VPNSHIFT;

    /*
     * 3. Obtain a pointer to the Support Structure of the current process.
     */
    support_t *currentSupport = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    if (currentSupport == NULL) {
        /* This shouldn't happen for a U-proc with virtual memory */
        SYSCALL(TERMINATEPROCESS, 0, 0, 0);
        return;
    }

    /*
     * 4. Access the Page Table of the current process from its Support Structure.
     */
    pageTableEntry_PTR pageTable = currentSupport->sup_pageTable;
    int asid = currentSupport->sup_asid;

    /*
     * 5. Search the Page Table for the PTE corresponding to the VPN.
     */
    if (vpn >= MAXPAGES) {
        /* Invalid VPN */
        SYSCALL(TERMINATEPROCESS, 0, 0, 0);
        return;
    }

    pageTableEntry_PTR pte = &pageTable[vpn];

    /*
     * 6. Check if the PTE is valid
     */
    if ((pte->pte_entryLO & VALIDON) == 0) {
        /* Page Fault: handle it by bringing the page into memory */
        
        /* Acquire swap pool semaphore to ensure mutual exclusion */
        SYSCALL(PASSEREN, &swapPoolSema4, 0, 0);
        
        /* Get next frame using FIFO */
        int frameNum;
        allocateFrame(&frameNum);
        
        /* Calculate physical address for the frame */
        memaddr frameAddr = FRAME_TO_ADDR(frameNum);
        
        /* Direct frame ownership check - no searching required */
        /* Simply check the swap table entry for this frame number */
        int i = frameNum;  /* In FIFO, frame number matches the swap table index */
        
        /* If this entry is valid, it means the frame is already in use */
        if (swapPoolData.swapTable[i].valid) {
            int oldAsid = swapPoolData.swapTable[i].asid;
            int oldVpn = swapPoolData.swapTable[i].vpn;
            
            /* Get old process's support structure */
            support_t *oldSupport = &supportStructures[oldAsid];
            
            /* Check if page is valid (still in memory) */
            if ((oldSupport->sup_pageTable[oldVpn].pte_entryLO & VALIDON)) {
                /* Save this page to flash */
                int flashDev = swapPoolData.swapTable[i].flash_dev;
                int blockNum = swapPoolData.swapTable[i].block_num;
                
                /* Write the page to flash */
                writeFlashPage(flashDev, blockNum, frameAddr);
                
                /* Mark page as invalid in page table */
                oldSupport->sup_pageTable[oldVpn].pte_entryLO &= ~VALIDON;
            }
        }
        
        /* Try to find a flash location for the new page */
        int flashDev, blockNum;
        if (findSwapEntry(asid, vpn, &flashDev, &blockNum) == SUCCESS) {
            /* Page already has a swap location, read it */
            readFlashPage(flashDev, blockNum, frameAddr);
        } else {
            /* New page, initialize with zeros and allocate swap entry */
            memaddr addr;
            for (addr = frameAddr; addr < frameAddr + PAGESIZE; addr += WORDLEN) {
                *((unsigned int *)addr) = 0;
            }
            
            /* Allocate swap entry */
            allocateSwapEntry(asid, vpn, &flashDev, &blockNum);
        }
        
        /* Update swap table entry for this frame */
        swapPoolData.swapTable[frameNum].asid = asid;
        swapPoolData.swapTable[frameNum].vpn = vpn;
        swapPoolData.swapTable[frameNum].valid = TRUE;
        
        /* Update the page table entry */
        pte->pte_entryLO = (frameNum << PFNSHIFT) | VALIDON | DIRTYON;
        
        SYSCALL(VERHOGEN, &swapPoolSema4, 0, 0);
    }

    /*
     * 7. If the PTE is valid, retrieve the Physical Frame Number (PFN) from the PTE.
     */
    unsigned int pfn = (pte->pte_entryLO & PTEFRAME) >> PFNSHIFT;

    /*
     * 8. Construct the EntryHi and EntryLo values for the TLB entry.
     */
    unsigned int entryHi = (vpn << VPNSHIFT) | (asid & 0xFF);
    
    /* Always include the dirty bit in EntryLo to allow writes
     * In PandOS, we generally want pages to be writable */
    unsigned int entryLo = pte->pte_entryLO;

    /*
     * 9. Write the new TLB entry.
     */
    setENTRYHI(entryHi);
    setENTRYLO(entryLo);
    TLBWR();

    /*
     * 10. Restore the processor state.
     */
    LDST((state_t *)BIOSDATAPAGE);
}

/* Following the recommendation in pandos.pdf [Section 4.11.2] and the previous conversation,
 * the functions for SYS16 (FLASH PUT) and SYS17 (FLASH GET) should ideally be moved
 * to deviceSupportDMA.c in Phase 4 / Level 5 to promote code sharing.
 * However, for Phase 3 / Level 4, they might initially reside here, called by the Pager.
 */