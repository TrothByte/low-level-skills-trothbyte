/*
 * GOOD: constant-time tag/secret comparison.
 * Reads every byte exactly once, folds the differences into one accumulator,
 * and only then branches on the result. The branch outcome depends only on
 * "equal or not", never on WHERE the difference is, so the timing channel
 * seen in bad/early_exit_secret.c is removed (see good/timing_demo.c).
 */
#include <stddef.h>
#include <stdint.h>

int ct_tag_equals(const uint8_t *expected, const uint8_t *given, size_t n) {
    uint8_t acc = 0;
    for (size_t i = 0; i < n; i++)
        acc |= (uint8_t)(expected[i] ^ given[i]);
    return acc == 0;
}
