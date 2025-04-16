/* 
Leap Year Tester
This program tests the leap year functionality by reading a year from the terminal,
checking if it is a leap year, and printing the result to the terminal.
It also includes a test for string length limits by attempting to print a long string.

Written by Aryah Rao

The intToStr and strToInt functions below were written together by Aryah Rao and Anish Reddy.
*/

#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

/* 
 * Converts a string to an integer
 * str: input string
 * len: length of the string
 * Returns the integer value represented by the string
 */
int strToInt(char *str, int len) {
    int i = 0;
    int result = 0;
    int sign = 1;
    if (str[0] == '-') { /* Check for negative sign */
        sign = -1;
        i = 1; 
    }
    for (; i < len && str[i] != '\n'; i++) {
        if ((str[i] >= '0') && (str[i] <= '9')) {
            result = result * 10 + (str[i] - '0'); /* Convert ASCII to integer */
        }
    }
    return (sign * result);
}

/*
 * Converts an integer to a string
 * num: integer to convert
 * str: output buffer to store the string representation
 */
void intToStr(int num, char *str) {
    int i = 0;
    int isNegative = 0;
    int temp;
    char buffer[20];
    if (num < 0) {  /* Check for negative number */
        isNegative = 1;
        num = -num; 
    }
    if (num == 0) { /* Handle 0 */
        str[0] = '0';
        str[1] = EOS;
        return; 
    }
    while (num > 0) { /* Convert digits in reverse order */
        buffer[i++] = (num % 10) + '0';
        num = num / 10;
    }
    if (isNegative) { /* Add negative sign if necessary */
        buffer[i++] = '-';
    }
    temp = 0;
    while (i > 0) { /* Reverse the string */
        str[temp++] = buffer[--i];
    }
    str[temp] = EOS; /* Null-terminate the string */
}

/* 
 * Checks if a given year is a leap year
 * year: year to check
 * Returns 1 if the year is a leap year, 0 otherwise
 */
int IsLeapYear(int year) {
    if (year % 4 != 0) /* Check if divisible by 4 */
        return 0;
    else if (year % 100 != 0) /* Check if divisible by 100 */
        return 1;
    else if (year % 400 != 0) /* Check if divisible by 400 */
        return 0;
    else
        return 1;
}

int main(void) {
    char inputBuffer[20];
    char outputBuffer[40];
    int status, year;

    print(WRITETERMINAL, "Leap Year Test starts\n");
    print(WRITETERMINAL, "Enter a year: ");

    status = SYSCALL(READTERMINAL, (int)&inputBuffer[0], 0, 0);
    inputBuffer[status] = EOS;
    year = strToInt(inputBuffer, status);

    if (IsLeapYear(year)) {
        intToStr(year, outputBuffer);
        print(WRITETERMINAL, outputBuffer);
        print(WRITETERMINAL, " is a leap year\n");
    } else {
        intToStr(year, outputBuffer);
        print(WRITETERMINAL, outputBuffer);
        print(WRITETERMINAL, " is not a leap year\n");
    }

    print(WRITETERMINAL, "Leap Year Test concluded\n");

    /* Test for string length limit */
    char longString[200];
    int i;
    for (i = 0; i < 200; i++) {
        longString[i] = 'X';
    }
    print(WRITETERMINAL, longString);

    print(WRITETERMINAL, "\nError: Process did not terminate when string was too long.\n");
    
    /* Terminate the process */
    SYSCALL(TERMINATE, 0, 0, 0);
    return 0;
}