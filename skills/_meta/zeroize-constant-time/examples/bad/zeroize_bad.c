// BAD: zeroization and constant-time mistakes (for teaching).
// Compile: gcc -O2 -S zeroize_bad.c
// Each function is a pattern the optimizer or a timing attack exploits.

#include <stddef.h>
#include <stdint.h>
#include <string.h>

// B1: stack-local wipe that the optimizer deletes.
// GCC 16.1 at -O2 compiles the body to just `ret` — the fill loop AND
// the memset are gone, so the secret survives in the stack slot.
void wipe_stack_buf(size_t n) {
    unsigned char secret[64];
    for (size_t i = 0; i < n && i < 64; i++) secret[i] = (unsigned char)i;
    memset(secret, 0, sizeof secret);
}

// B2: wipe through a plain pointer, relying on "caller cannot see it".
// GCC keeps the call here (jmp memset) only because the pointer may be
// observed; the moment the buffer provably dies the call disappears.
void wipe_heap_buf(unsigned char *secret, size_t n) {
    memset(secret, 0, n);
}

// B3: secret compare with early exit.
// The loop returns at the first differing byte; runtime leaks the
// position of the first difference. asm shows cmpb + je on secret bytes.
int ct_memcmp_bad(const unsigned char *a, const unsigned char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (a[i] != b[i]) return 0;   /* secret-dependent branch */
    }
    return 1;
}

// B4: secret-dependent branch. May compile to sete/cmov here, but the
// source is not constant-time by construction — a more complex body or
// another compiler keeps a real branch whose timing leaks.
int secret_eq_bad(uint32_t secret, uint32_t guess) {
    if (secret == guess) return 1;    /* data-dependent decision */
    return 0;
}

// B5: secret-dependent memory index (cache-timing channel).
// table[secret_idx] makes the accessed address depend on the secret.
unsigned char secret_index_bad(const unsigned char *table, size_t secret_idx) {
    return table[secret_idx];
}
