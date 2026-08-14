// GOOD: reliable zeroization and constant-time code.
// Compile: gcc -O2 -S zeroize_good.c
// Verify: grep the asm for the wipe stores and for branches.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// G1: volatile-sink zeroization.
// Writes through a volatile pointer are observable side effects, so the
// loop cannot be elided. GCC 16.1 -O2 emits a real movb $0 store loop.
void secure_zero_memory(void *ptr, size_t len) {
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (len--) *p++ = 0;
}

// G2: explicit_bzero-style wipe (memset anchored by a compiler barrier).
// The volatile asm with "memory" clobber makes the memset observable.
void explicit_bzero_style(void *ptr, size_t len) {
    memset(ptr, 0, len);
    __asm__ __volatile__("" : : "r"(ptr), "r"(len) : "memory");
}

// G3: wipe a stack-local secret after last use.
// Unlike the bad version, the wipe store survives at -O2.
int wipe_stack_after_use(int ok) {
    unsigned char secret[16];
    for (int i = 0; i < 16; i++) secret[i] = (unsigned char)(ok + i);
    int r = 0;
    for (int i = 0; i < 16; i++) r += secret[i];
    secure_zero_memory(secret, sizeof secret);
    return r;
}

// G4: constant-time compare (XOR-accumulate, single final test).
// No early exit, no secret-dependent branch; asm shows pxor/por and one
// final testb/sete instead of a per-byte je on secret data.
int ct_memcmp_good(const unsigned char *a, const unsigned char *b, size_t n) {
    unsigned char diff = 0;
    for (size_t i = 0; i < n; i++) {
        diff |= a[i] ^ b[i];
    }
    return diff == 0;
}

// G5: constant-time selection with bitwise ops.
// mask = all-ones iff cond != 0; result is a or b chosen arithmetically.
// asm: and/andn/or (or a cmov); no branch on the secret condition.
uint32_t ct_select_good(uint32_t a, uint32_t b, int cond) {
    uint32_t mask = 0u - (uint32_t)(cond != 0);
    return (a & mask) | (b & ~mask);
}
