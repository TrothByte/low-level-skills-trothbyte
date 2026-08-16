// BAD: secret-dependent branch in a "constant-time" claim. The early exit
// on a secret byte leaks the first-difference position (CWE-1254), and the
// secret is used to index a table (cache-address leak).
// intentionally incorrect
#include <stdint.h>
#include <stdio.h>

static uint8_t sbox[256];

// BAD: returns 1 on first equal byte — timing depends on WHERE the bytes
// differ, leaking secret information.
static int ct_claim(const uint8_t *secret, const uint8_t *pub, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (secret[i] != pub[i]) {   // BAD: branch on secret data
            return 0;
        }
    }
    return 1;
}

// BAD: secret used as a table index — address-dependent memory access leaks
// through the cache hierarchy (the AES T-table class).
static uint8_t lookup(const uint8_t *secret, size_t i) {
    return sbox[secret[i]];          // BAD: secret index -> cache leak
}

int main(void) {
    for (int i = 0; i < 256; i++) sbox[i] = (uint8_t)(i * 3);
    uint8_t secret[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint8_t pub[8]    = {1, 2, 9, 4, 5, 6, 7, 8};

    volatile int r = ct_claim(secret, pub, 8);
    volatile uint8_t v = lookup(secret, 0);
    printf("result=%d (leaky: early-exit + secret-indexed table)\n", r);
    printf("lookup=%u\n", (unsigned)v);
    return 0;
}
