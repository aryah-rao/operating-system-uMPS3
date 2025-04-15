/* Tests integer addition operations*/
/* intToStr() and strToInt() are used by addition.c and subtraction.c */

#include "h/localLibumps.h"
#include "h/tconst.h"
#include "h/print.h"

/* Convert integer to string for printing*/
void intToStr(int num, char *str)
{
    int i = 0;
    int isNegative = FALSE;
    int temp;
    char buffer[20];

    /* Handle negative numbers */
    if (num < 0)
    {
        isNegative = TRUE;
        num = -num;
    }

    /* Handle special case of 0 */
    if (num == 0)
    {
        str[0] = '0';
        str[1] = EOS;
        return;
    }

    /* Convert digits in reverse order */
    while (num > 0)
    {
        buffer[i++] = (num % 10) + '0';
        num = num / 10;
    }

    /* Add negative sign if needed */
    if (isNegative)
    {
        buffer[i++] = '-';
    }

    /* Reverse the string */
    temp = 0;
    while (i > 0)
    {
        str[temp++] = buffer[--i];
    }
    str[temp] = EOS;
}

/* Convert string to integer */
int strToInt(char *str, int len)
{
    int i = 0;
    int result = 0;
    int sign = 1;

    /* Check for negative sign */
    if (str[0] == '-')
    {
        sign = -1;
        i = 1;
    }

    /* Parse digits */
    for (; i < len && str[i] != '\n'; i++)
    {
        if (str[i] >= '0' && str[i] <= '9')
        {
            result = result * 10 + (str[i] - '0');
        }
    }

    return sign * result;
}

void main()
{
    int num1, num2, result;
    char inputBuffer[20];
    char outputBuffer[40];
    int status;

    print(WRITETERMINAL, "Addition Test starts\n");

    /* Test 1: User input addition */
    print(WRITETERMINAL, "Enter first number: ");
    status = SYSCALL(READTERMINAL, (int)&inputBuffer[0], 0, 0);
    inputBuffer[status] = EOS;
    num1 = strToInt(inputBuffer, status);

    print(WRITETERMINAL, "Enter second number: ");
    status = SYSCALL(READTERMINAL, (int)&inputBuffer[0], 0, 0);
    inputBuffer[status] = EOS;
    num2 = strToInt(inputBuffer, status);

    result = num1 + num2;

    print(WRITETERMINAL, "Result of ");
    intToStr(num1, outputBuffer);
    print(WRITETERMINAL, outputBuffer);
    print(WRITETERMINAL, " + ");
    intToStr(num2, outputBuffer);
    print(WRITETERMINAL, outputBuffer);
    print(WRITETERMINAL, " = ");
    intToStr(result, outputBuffer);
    print(WRITETERMINAL, outputBuffer);
    print(WRITETERMINAL, "\n\n");

    print(WRITETERMINAL, "\nAddition Test concluded\n");

    /* Terminate normally */
    SYSCALL(TERMINATE, 0, 0, 0);
}
