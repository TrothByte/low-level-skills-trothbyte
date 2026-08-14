// BAD: unbounded strcpy/strcat on user input (STR31-C).
// Compile: gcc -Wall -Wextra -O2 -Wno-error -o strcpy_bad strcpy_bad.c
// Expect warning: -Wstringop-overflow
#include <stdio.h>
#include <string.h>

int main(void) {
    char name[16];
    const char *user_input = "01234567890123456789";
    strcpy(name, user_input);           // overflow: input is 20 bytes, buffer is 16
    strcat(name, ".log");               // second unbounded append, worse
    printf("name=%s\n", name);
    return 0;
}
