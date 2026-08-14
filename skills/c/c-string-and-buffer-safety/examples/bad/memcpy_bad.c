// BAD: overlapping memcpy is UB; memmove is required.
// Compile: gcc -Wall -Wextra -O2 -Wno-error -o memcpy_bad memcpy_bad.c
#include <stdio.h>
#include <string.h>

int main(void) {
    char buf[16];
    memcpy(buf, "abcdefghij", 11);      // 10 chars + NUL
    memcpy(buf + 1, buf, 10);           // overlapping regions -> UB (memcpy-param-overlap)
    printf("buf=%s\n", buf);            // result may be corrupt
    return 0;
}
