/* 
Arithmetic Operations Tester
This program tests the addition and subtraction functionality by reading two integers from the terminal,
performing addition and subtraction, and printing the results to the terminal.

Written by Anish Reddy

The intToStr and strToInt functions below were written together by Aryah Rao and Anish Reddy.
*/

#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

/* 
 * Converts a string to an integer.
 * str: input string
 * len: length of the string
 * Returns the integer value represented by the string.
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
 * Converts an integer to a string.
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

int main(void) {
    int num1, num2, result1, result2;
    char inputBuffer[20];
    char outputBuffer[40];
    int status;

    print(WRITETERMINAL, "Addition and Subtraction Test starts\n");

    /* Addition Test */
    print(WRITETERMINAL, "Enter first number: ");
    status = SYSCALL(READTERMINAL, (int)&inputBuffer[0], 0, 0);
    inputBuffer[status] = EOS;
    num1 = strToInt(inputBuffer, status);

    print(WRITETERMINAL, "Enter second number: ");
    status = SYSCALL(READTERMINAL, (int)&inputBuffer[0], 0, 0);
    inputBuffer[status] = EOS;
    num2 = strToInt(inputBuffer, status);

    result1 = num1 + num2;

    print(WRITETERMINAL, "Result of ");
    outputBuffer[0] = EOS;
    intToStr(num1, outputBuffer);
    print(WRITETERMINAL, outputBuffer);
    print(WRITETERMINAL, " + ");
    outputBuffer[0] = EOS;
    intToStr(num2, outputBuffer);
    print(WRITETERMINAL, outputBuffer);
    print(WRITETERMINAL, " = ");
    outputBuffer[0] = EOS;
    intToStr(result1, outputBuffer);
    print(WRITETERMINAL, outputBuffer);
    print(WRITETERMINAL, "\n\n");

    print(WRITETERMINAL, "Addition Test concluded\n\n");

    /* Subtraction Test */
    result2 = num1 - num2;

    print(WRITETERMINAL, "Result of ");
    outputBuffer[0] = EOS;
    intToStr(num1, outputBuffer);
    print(WRITETERMINAL, outputBuffer);
    print(WRITETERMINAL, " - ");
    outputBuffer[0] = EOS;
    intToStr(num2, outputBuffer);
    print(WRITETERMINAL, outputBuffer);
    print(WRITETERMINAL, " = ");
    outputBuffer[0] = EOS;
    intToStr(result2, outputBuffer);
    print(WRITETERMINAL, outputBuffer);
    print(WRITETERMINAL, "\n\n");

    print(WRITETERMINAL, "Subtraction Test concluded\n");

    /* Terminate normally */
    SYSCALL(TERMINATE, 0, 0, 0);
    return 0;
}