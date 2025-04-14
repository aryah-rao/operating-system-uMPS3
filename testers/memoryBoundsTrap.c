/* 
 * Memory Bounds Trap Test
 *
 * This test demonstrates memory allocation, pattern writing/reading,
 * and intentionally terminates by attempting to print an excessively
 * long string (which exceeds the MAXSTRINGLEN limit in the OS).
 */

#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

/* Size of our test memory area */
#define MEMORY_SIZE 50
/* Pattern to write to memory */
#define PATTERN_A 0xAA
#define PATTERN_B 0x55

void main() {
    int i;
    unsigned char memory[MEMORY_SIZE];
    int errors = 0;
    
    print(WRITETERMINAL, "Memory Test Starting...\n");
    
    /* Step 1: Write alternating bit patterns to memory */
    print(WRITETERMINAL, "Writing patterns to memory...\n");
    for (i = 0; i < MEMORY_SIZE; i++) {
        if (i % 2 == 0) {
            memory[i] = PATTERN_A;
        } else {
            memory[i] = PATTERN_B;
        }
    }
    
    /* Step 2: Read back and verify */
    print(WRITETERMINAL, "Verifying memory contents...\n");
    for (i = 0; i < MEMORY_SIZE; i++) {
        unsigned char expected = (i % 2 == 0) ? PATTERN_A : PATTERN_B;
        if (memory[i] != expected) {
            errors++;
        }
    }
    
    /* Step 3: Report results */
    if (errors == 0) {
        print(WRITETERMINAL, "Memory test successful! All patterns verified.\n");
    } else {
        print(WRITETERMINAL, "Memory test failed with errors!\n");
    }
    
    print(WRITETERMINAL, "\nNow attempting to calculate first 10 Fibonacci numbers:\n");
    
    /* Step 4: Calculate and display Fibonacci numbers */
    int fib[10];
    fib[0] = 1;
    fib[1] = 1;
    
    print(WRITETERMINAL, "Fib(1) = 1\n");
    print(WRITETERMINAL, "Fib(2) = 1\n");
    
    for (i = 2; i < 10; i++) {
        fib[i] = fib[i-1] + fib[i-2];
        
        /* Create a small buffer to display each result */
        char buffer[20];
        buffer[0] = 'F';
        buffer[1] = 'i';
        buffer[2] = 'b';
        buffer[3] = '(';
        buffer[4] = '0' + (i + 1);  /* Simple conversion for single digit */
        buffer[5] = ')';
        buffer[6] = ' ';
        buffer[7] = '=';
        buffer[8] = ' ';
        
        /* Very simple conversion of number to string */
        int num = fib[i];
        int pos = 9;
        
        if (num == 0) {
            buffer[pos++] = '0';
        } else {
            /* Find number of digits */
            int temp = num;
            int digits = 0;
            while (temp > 0) {
                temp /= 10;
                digits++;
            }
            
            /* Convert digits */
            pos += digits - 1;
            temp = pos;
            while (num > 0) {
                buffer[temp--] = '0' + (num % 10);
                num /= 10;
            }
            pos++;
        }
        
        buffer[pos++] = '\n';
        buffer[pos] = '\0';
        
        print(WRITETERMINAL, buffer);
    }
    
    print(WRITETERMINAL, "\nAll tests completed successfully!\n");
    print(WRITETERMINAL, "Now attempting to print a string that's too long...\n");
    
    /* Step 5: Create a very long string (exceeding MAXSTRINGLEN) */
    char longString[200];
    for (i = 0; i < 199; i++) {
        longString[i] = 'X';
    }
    longString[199] = '\0';
    
    /* This print should cause the OS to terminate the process */
    print(WRITETERMINAL, longString);
    
    /* We should never reach here */
    print(WRITETERMINAL, "ERROR: Process was not terminated!\n");
    SYSCALL(TERMINATE, 0, 0, 0);
}
