// BAD: strncpy does not NUL-terminate when src fills the buffer (STR32-C).
// Compile: gcc -Wall -Wextra -O2 -Wno-error -o strncpy_bad strncpy_bad.c
// Expect warning: -Wstringop-truncation
#include <stdio.h>
#include <string.h>

int main(void) {
    char buf[8];
    strncpy(buf, "0123456789", sizeof buf);   // copies 8 bytes, no room for the NUL
    size_t len = strlen(buf);                 // over-read: scans past the 8-byte buffer
    printf("len=%zu\n", len);
    return 0;
}
