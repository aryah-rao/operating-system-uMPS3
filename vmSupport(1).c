/*
 * vmSupport.c - Implementation of the Virtual Memory Support module (Level 4 / Phase 3).
 *
 * This module is responsible for providing virtual memory capabilities in the Pandos
 * operating system. It includes the Translation Lookaside Buffer (TLB) exception
 * handler (the Pager), initial functions for interacting with flash devices for
 * backing store, and management of the Swap Pool.
 *
 * Authors: [Your Names Here]
 * Date: [Current Date]
 */

#include "../h/vmSupport.h" // To access the support structure and page table

// Module-wide private variables
HIDDEN struct {
    // Implementation of the Swap Pool table. This table will track the mapping
    // between virtual pages and their location in the swap space (e.g., flash device blocks).
    // The structure of this table is implementation-dependent.
    // For example, it could be an array of entries, where each entry contains:
    // - The ASID of the process the page belongs to.
    // - The virtual page number.
    // - The flash device number.
    // - The block number on the flash device.
    // - Potentially status bits (e.g., valid, dirty).
    // The size of this table will determine the maximum size of the swap space.
    // Consider using MAXPROC for the number of possible concurrent processes
    // and a suitable number of entries per process based on the logical address space.
    // Example:
    // struct swap_pool_entry {
    //     int asid;
    //     int vpn;
    //     int flash_dev;
    //     int block_num;
    //     int valid;
    //     int dirty;
    // };
    // struct swap_pool_entry swapPoolTable[NUM_SWAP_ENTRIES];
    // ... (Define the actual structure and size)
} swapPoolData;

// Public function to initialize the Swap Pool table and semaphore.
// This function will be called by the InstantiatorProcess (likely in initProc.c).
void initSwapStructs() {
    /*
     * Initialize the Swap Pool table.
     * This involves setting up the initial state of all entries in the table,
     * potentially marking them as invalid or available.
     * Consider iterating through the table and initializing relevant fields.
     */
    // Implementation of Swap Pool table initialization

    /*
     * Initialize the Swap Pool semaphore.
     * This semaphore will be used to ensure mutual exclusive access to the
     * Swap Pool data structures, especially when the Pager needs to allocate
     * or deallocate swap space. Initialize it to 1 (unlocked).
     */
    swapPoolSemaphore = 1; // Initial value for a binary semaphore
}

// HIDDEN helper function to allocate a free Swap Pool entry.
// This function needs to find an unused entry in the swapPoolTable.
// It should acquire the swapPoolSemaphore before accessing the table.
HIDDEN int allocateSwapEntry(int asid, int vpn, int *flashDev, int *blockNum) {
    /*
     * Acquire the Swap Pool semaphore (SYS3).
     */
    SYSCALL(GET_MUTEX, &swapPoolSemaphore, 0, 0);

    /*
     * Search the swapPoolTable for a free entry.
     * A free entry might be one that is marked as invalid or belongs to a terminated process.
     * If a suitable entry is found, mark it as used by the current process (ASID)
     * and virtual page number (VPN), and set the flash device and block number.
     * Return 0 on success and -1 if no free entry is found.
     */
    // Implementation of finding and allocating a swap entry

    /*
     * Release the Swap Pool semaphore (SYS4).
     */
    SYSCALL(RELEASE_MUTEX, &swapPoolSemaphore, 0, 0);

    return -1; // Placeholder for return value
}

// HIDDEN helper function to deallocate a Swap Pool entry.
// This function should mark a swap entry as free.
// It should acquire the swapPoolSemaphore before accessing the table.
HIDDEN void deallocateSwapEntry(int asid, int vpn) {
    /*
     * Acquire the Swap Pool semaphore (SYS3).
     */
    SYSCALL(GET_MUTEX, &swapPoolSemaphore, 0, 0);

    /*
     * Search the swapPoolTable for the entry corresponding to the given ASID and VPN.
     * Mark this entry as free (e.g., set the valid bit to 0).
     */
    // Implementation of finding and deallocating a swap entry

    /*
     * Release the Swap Pool semaphore (SYS4).
     */
    SYSCALL(RELEASE_MUTEX, &swapPoolSemaphore, 0, 0);
}

// HIDDEN function to read a page from the flash device.
// This function will be used by the Pager to bring a page from the backing store into RAM.
HIDDEN int readFlashPage(int flashDev, int blockNum, memaddr physicalAddress) {
    /*
     * Initiate a synchronous read operation on the specified flash device and block number.
     * This will involve using the SYS17 (FLASH GET) system call.
     * The data read from the flash device should be placed at the provided physicalAddress in RAM.
     * Return the completion status of the operation.
     * Handle potential errors (e.g., invalid flash device or block number) - though
     * such checks might be done by the caller (Pager).
     */
    return SYSCALL(FLASH_GET, (unsigned int *)physicalAddress, flashDev, blockNum);
}

// HIDDEN function to write a page to the flash device.
// This function will be used by the Pager to write a dirty page to the backing store.
HIDDEN int writeFlashPage(int flashDev, int blockNum, memaddr physicalAddress) {
    /*
     * Initiate a synchronous write operation on the specified flash device and block number.
     * This will involve using the SYS16 (FLASH PUT) system call.
     * The data at the provided physicalAddress in RAM should be written to the flash device.
     * Return the completion status of the operation.
     * Handle potential errors (e.g., invalid flash device or block number) - though
     * such checks might be done by the caller (Pager).
     */
    return SYSCALL(FLASH_PUT, (unsigned int *)physicalAddress, flashDev, blockNum);
}

// TLB exception handler (The Pager).
void uTLB_RefillHandler() {
    /*
     * This function is invoked by the hardware when a TLB miss occurs.
     * It is a Level 3/Phase 2 handler that executes in kernel mode with interrupts disabled
     * and uses the Nucleus stack. It has access to Level 3 global structures (e.g., Current Process)
     * and also to the process's Support Structure (e.g., Page Table).
     */

    /*
     * 1. Determine the virtual address that caused the TLB miss.
     *    This can be obtained from the BadVAddr CP0 register (getBADVADDR()).
     */
    unsigned int badVAddr = getBADVADDR();

    /*
     * 2. Extract the Virtual Page Number (VPN) from the BadVAddr.
     *    Remember the address format: 20-bit VPN | 12-bit Offset.
     */
    unsigned int vpn = badVAddr >> 12;

    /*
     * 3. Obtain a pointer to the Support Structure of the current process.
     *    This can be done using SYS8 (GETSUPPORTPTR).
     */
    support_t *currentSupport = (support_t *)SYSCALL(GETSUPPORTPTR, 0, 0, 0);
    if (currentSupport == NULL) {
        // This should ideally not happen for a U-proc with virtual memory enabled.
        // Handle this error, possibly by terminating the process (SYS9).
        PANIC(); // Or a more graceful termination
        return;
    }

    /*
     * 4. Access the Page Table of the current process from its Support Structure.
     *    The Page Table is an array of 32 Page Table Entries (PTEs).
     */
    pte_t *pageTable = currentSupport->sup_pageTable; // Assuming 'sup_pageTable' is the name in support_t

    /*
     * 5. Search the Page Table for the PTE corresponding to the VPN.
     *    The VPN can be used as an index into the Page Table (0 to 31).
     *    Ensure the VPN is within the valid range.
     */
    if (vpn < 0 || vpn >= 32) {
        // Invalid VPN - this should not happen under normal circumstances.
        // Handle this error, possibly by terminating the process (SYS9) or raising a Program Trap.
        PANIC(); // Or a more graceful termination
        return;
    }

    pte_t *pte = &pageTable[vpn];

    /*
     * 6. Check if the PTE is valid (e.g., a 'valid' bit is set).
     */
    if (!pte->valid) {
        /*
         * Page Fault: The requested page is not in memory.
         * Perform the following steps:
         *   a. Allocate a free physical frame from the frame pool (if available).
         *   b. If no free frame is available, perform page replacement:
         *      i. Select a page to evict (e.g., using a replacement algorithm like LRU or FIFO).
         *      ii. If the evicted page is dirty, write it back to the backing store (flash device).
         *      iii. Update the PTE of the evicted page to mark it as invalid and point to its new location in the swap space.
         *      iv. Deallocate the physical frame.
         *   c. Allocate the newly freed (or originally free) physical frame.
         *   d. Determine the location of the requested page in the backing store (flash device).
         *      This information might be stored in the Swap Pool table based on the VPN and ASID.
         *   e. Read the requested page from the backing store into the allocated physical frame using readFlashPage().
         *   f. Update the PTE for the requested virtual page:
         *      i. Set the Page Frame Number (PFN) to the physical frame number.
         *      ii. Mark the PTE as valid.
         *      iii. Set the dirty bit to 0 (initially, as it's just loaded from disk).
         */
        // Implementation of Page Fault handling (frame allocation, page replacement, reading from flash)
    }

    /*
     * 7. If the PTE is valid, retrieve the Physical Frame Number (PFN) from the PTE.
     */
    unsigned int pfn = pte->pfn;

    /*
     * 8. Construct the EntryHi and EntryLo values for the TLB entry.
     *    EntryHi.VPN = VPN
     *    EntryHi.ASID = Current Process's ASID (from currentSupport->sup_asid)
     *    EntryLo.PFN = PFN
     *    EntryLo.Valid = 1
     *    EntryLo.Dirty = PTE->dirty (from the PTE)
     *    EntryLo.Global = 0 (assuming non-global pages for user processes)
     */
    unsigned int entryHi = (vpn << 13) | (currentSupport->sup_asid & 0xFF); // Assuming VPN is top 20 bits, ASID is lower 8
    unsigned int entryLo = (pfn << 6) | (pte->dirty ? 0x4 : 0x0) | 0x1;     // PFN top 20, Dirty bit, Valid bit

    /*
     * 9. Write the new TLB entry using TLBWR (TLB Write Random) or TLBWI (TLB Write Indexed).
     *    TLBWR writes to a random entry, while TLBWI writes to the entry indexed by the Index register.
     *    For simplicity in initial implementation, TLBWR is often used.
     */
    setENTRYHI(entryHi);
    setENTRYLO(entryLo);
    TLBWR(); // Or TLBWI if using an index

    /*
     * 10. Restore the processor state using LDST (Load State).
     *     The saved exception state is typically located at the beginning of the BIOS Data Page (0x0FFFF000).
     */
    LDST((state_PTR)0x0FFFF000);
}

// Following the recommendation in pandos.pdf [Section 4.11.2] and the previous conversation,
// the functions for SYS16 (FLASH PUT) and SYS17 (FLASH GET) should ideally be moved
// to deviceSupportDMA.c in Phase 4 / Level 5 to promote code sharing.
// However, for Phase 3 / Level 4, they might initially reside here, called by the Pager.

// This module should also contain the Swap Pool table and its associated semaphore,
// as well as functions for managing the Swap Pool (allocateSwapEntry, deallocateSwapEntry).