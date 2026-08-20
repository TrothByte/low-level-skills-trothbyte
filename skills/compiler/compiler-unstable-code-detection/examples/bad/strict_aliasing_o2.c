// BAD: strict-aliasing violation inside a function with int* and float*
// parameters. GCC must assume the two pointer types do not alias (C11
// 6.5p7), so it may keep `*a == 1` without reloading after the float store.
//
// Verified divergence (gcc 16.1 MinGW, this host):
//   gcc -O0  -> "r=1075838976"   (bits of 2.5f written into the int slot,
//                                 then reloaded as int)
//   gcc -O2  -> "r=1"            (the int store is kept; the float store is
//                                 assumed not to touch the int object)
//
// Same-storage punning through a union is fine on this host (MinGW gcc
// handles union punning as defined); this pointer-based violation is what
// actually diverges here. A single-threaded simple punning example does NOT
// diverge on gcc 16.1 — the function boundary is what forces the TBAA split.

#include <stdint.h>
#include <stdio.h>

static int my_function(int *a, float *b) {
    *a = 1;
    *b = 2.5f;
    return *a;
}

int main(void) {
    uint32_t storage;
    int *a = (int *)&storage;
    float *b = (float *)&storage;
    int r = my_function(a, b);
    printf("r=%d\n", r);
    return 0;
}
