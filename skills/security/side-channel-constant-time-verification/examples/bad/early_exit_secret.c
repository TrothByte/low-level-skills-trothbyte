/*
 * BAD: // intentionally incorrect — secret-dependent early-exit comparison.
 * Compares a password/hmac-style tag byte-by-byte and returns at the first
 * difference. The number of comparisons performed depends on the secret, so a
 * local or remote attacker can recover it byte-by-byte via timing.
 * Demonstrated by good/timing_demo.c: first-byte-diff vs last-byte-diff of an
 * early-exit compare differ by 0.05 s across 500k iters on this host.
 */
#include <stddef.h>
#include <string.h>

int tag_equals(const char *expected, const char *given, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (expected[i] != given[i])
            return 0; /* leaks the first differing byte index via timing */
    }
    return 1;
}
