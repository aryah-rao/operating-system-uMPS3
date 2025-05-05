/******************************* aryahAnishTest.c *************************************
 *
 * Module: aryahAnishTest.c
 *
 * Tests SYSCALLs 14-18 (DISK_PUT, DISK_GET, FLASH_PUT, FLASH_GET, DELAY).
 * 
 * Written by Aryah Rao & Anish Reddy
 * 
 * This test performs the following:
 * - Tests DISK_PUT/GET with a specific disk and sector
 * - Tests FLASH_PUT/GET with a specific flash and block
 * - Tests DELAY with a specified number of seconds
 * - Verifies the data written to and read from the disk and flash
 * - Attempts to terminate the process via invalid syscalls (should result in termination)
 * 
 */

#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"
#include "../h/const.h"
#include "../h/types.h"

/* Test constants */
#define DISK_NUM 1
#define DISK_SECTOR 10
#define FLASH_NUM 1
#define FLASH_BLOCK 40      /* Must be >= 32 */
#define DELAY_SECONDS 3
#define BUFFER_INTS (PAGESIZE / sizeof(unsigned int))
#define INVALID_FLASH_BLOCK 5 /* Block number < 32 */
#define INVALID_DISK_NUM 0    /* DISK 0 is protected */
#define INVALID_DELAY_SECONDS -5

/* Verification function */
int verify_buffer(unsigned int *buffer, unsigned int expected_first, unsigned int expected_last, const char *test_name) {
    int ok = 1;
    int i;
    if (buffer[0] != expected_first || buffer[BUFFER_INTS - 1] != expected_last) {
        print(WRITETERMINAL, "ERROR: ");
        print(WRITETERMINAL, (char*)test_name);
        print(WRITETERMINAL, " failed!\n");
        ok = 0;
    } else {
        print(WRITETERMINAL, "SUCCESS: ");
        print(WRITETERMINAL, (char*)test_name);
        print(WRITETERMINAL, " passed!\n");
    }
    /* Clear buffer */
    for (i = 0; i < BUFFER_INTS; ++i) {
        buffer[i] = 0;
    }
    return ok;
}


int main() {
    int i;
    static unsigned int buffer[BUFFER_INTS];
    unsigned int pattern1_first = 0xAAAAAAAA;
    unsigned int pattern1_last = pattern1_first + BUFFER_INTS - 1;
    unsigned int pattern2_first = 0x77777777;
    unsigned int pattern2_last = pattern2_first - (BUFFER_INTS - 1);

    print(WRITETERMINAL, "Starting DMA/Delay Test (aryahAnishTest)\n");

    /* Test DISK I/O */
    print(WRITETERMINAL, "Testing DISK_PUT/GET\n");
    for (i = 0; i < BUFFER_INTS; ++i) {
        buffer[i] = pattern1_first + i;
    }
    print(WRITETERMINAL, "Writing to DISK 1\n");
    SYSCALL(DISK_PUT, (memaddr) buffer, DISK_NUM, DISK_SECTOR);

    for (i = 0; i < BUFFER_INTS; ++i) {
        buffer[i] = 0;
    }
    print(WRITETERMINAL, "Reading from DISK 1\n");
    SYSCALL(DISK_GET, (memaddr)buffer, DISK_NUM, DISK_SECTOR);
    verify_buffer(buffer, pattern1_first, pattern1_last, "DISK_PUT/GET");


    /* Test FLASH I/O */
    print(WRITETERMINAL, "Testing FLASH_PUT/GET\n");
    for (i = 0; i < BUFFER_INTS; ++i) {
        buffer[i] = pattern2_first - i;
    }
    print(WRITETERMINAL, "Writing to FLASH 1\n");
    SYSCALL(FLASH_PUT, (memaddr)buffer, FLASH_NUM, FLASH_BLOCK);

    for (i = 0; i < BUFFER_INTS; ++i) {
        buffer[i] = 0;
    }
    print(WRITETERMINAL, "Reading from FLASH 1\n");
    SYSCALL(FLASH_GET, (memaddr)buffer, FLASH_NUM, FLASH_BLOCK);
    verify_buffer(buffer, pattern2_first, pattern2_last, "FLASH_PUT/GET");


    /* Test DELAY */
    print(WRITETERMINAL, "Testing DELAY\n");
    print(WRITETERMINAL, "Sleeping for 3 seconds\n");
    cpu_t start = SYSCALL(GET_TOD, 0, 0, 0);
    SYSCALL(DELAY, DELAY_SECONDS, 0, 0);
    cpu_t end = SYSCALL(GET_TOD, 0, 0, 0);
    cpu_t elapsed = end - start;
    if (elapsed < DELAY_SECONDS * 1000) {
        print(WRITETERMINAL, "ERROR: DELAY failed!\n");
    } else {
        print(WRITETERMINAL, "SUCCESS: DELAY passed!\n");
    }

    print(WRITETERMINAL, "Tests finished.\n");
    print(WRITETERMINAL, "Thank you for everything Dr. G! :D\n");

    /* Test Termination via Invalid FLASH_PUT */
    SYSCALL(FLASH_PUT, (memaddr)buffer, FLASH_NUM, INVALID_FLASH_BLOCK);
    print(WRITETERMINAL, "ERROR: Process was not terminated by invalid FLASH_PUT!\n");

    /* Test Termination via Invalid DISK_PUT */
    SYSCALL(DISK_PUT, (memaddr)buffer, INVALID_DISK_NUM, DISK_SECTOR);
    print(WRITETERMINAL, "ERROR: Process was not terminated by invalid DISK_PUT!\n");

    /* Test Termination via Invalid DELAY */
    SYSCALL(DELAY, INVALID_DELAY_SECONDS, 0, 0);
    print(WRITETERMINAL, "ERROR: Process was not terminated by invalid DELAY!\n");

    /* Termination */
    SYSCALL(TERMINATE, 0, 0, 0);

    /* Should not be reached */
    print(WRITETERMINAL, "ERROR: Process should have terminated!\n");
    return 0;
}
