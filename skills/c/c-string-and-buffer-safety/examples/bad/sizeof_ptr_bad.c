// BAD: sizeof(decayed pointer) gives 8, not the buffer size (ARR01-C).
// Compile: gcc -Wall -Wextra -O2 -Wno-error -o sizeof_ptr_bad sizeof_ptr_bad.c
// Expect warning: -Wsizeof-pointer-memaccess
#include <stdio.h>
#include <string.h>

static char global_dst[64];

void fill(char dst[]) {                 // dst decays to char * inside
    memset(dst, 'x', sizeof dst);       // sizeof dst == sizeof(char*) == 8
}

int main(void) {
    fill(global_dst);                   // only 8 bytes written, caller expected 64
    printf("global_dst[0]=%c global_dst[63]=%d\n",
           global_dst[0], global_dst[63]);  // 63 is uninitialized -> indeterminate
    return 0;
}
