// BAD: the declared bound does not match reality — a "lying" count.
// Checked C equivalent of this bug:
//   _Array_ptr<int> data : count(8);   // declared 8 elements ...
//   ... on an object that really holds 4  (the checker would have to prove
//   count(8) only if it could; here the lie is a stack struct, so plain C
//   compiles it silently and the write overflows).
//
// This is the plain-C version of annotation mistake #2 from SKILL.md
// ("bounds that don't match reality"). It compiles AND overruns under gcc.
//
// Compile: gcc -Wall -Wextra -O2 -o bounds_lying bounds_lying.c
// Run:     ./bounds_lying
// Expect:  a -Wstringop-overflow warning at -O2 and a corrupted canary.
#include <stdio.h>
#include <string.h>

typedef struct {
    int data[4];              // real capacity: 4 ints
    unsigned long long canary;   // placed right after data to observe overflow
} bag;

static void load(bag *b, const int *src, size_t claimed) {
    // The "annotation" the migration proposed: _Array_ptr<int> data : count(claimed)
    memcpy(b->data, src, claimed * sizeof(int));
}

int main(void) {
    bag b = {{0, 0, 0, 0}, 0xDEADBEEFULL};
    int src[8];
    for (int i = 0; i < 8; ++i) {
        src[i] = i + 1;
    }

    load(&b, src, 8);   // claimed 8 > capacity 4: the bound lies -> overflow

    printf("canary=0x%llx (expected 0xdeadbeef)\n", b.canary);
    if (b.canary != 0xDEADBEEFULL) {
        printf("OVERFLOW DETECTED: canary corrupted\n");
        return 1;
    }
    printf("canary intact\n");
    return 0;
}
