/*
 * GOOD: secret-indexed lookup replaced by a constant-time bit-slice select.
 * No address derived from the secret: every candidate is read, and a
 * data-independent selection picks the winner. Branch-free AND/OR keeps the
 * control flow independent of the secret as well.
 */
#include <stddef.h>
#include <stdint.h>

static uint8_t ct_select(uint8_t bit, uint8_t a, uint8_t b) {
    uint8_t mask = (uint8_t)(0u - bit); /* 0xFF when bit=1, 0 when bit=0 */
    return (a & mask) | (b & ~mask);
}

uint8_t ct_sbox_lookup(const uint8_t *sbox, uint8_t idx) {
    uint8_t out = 0;
    for (size_t i = 0; i < 256; i++)
        out = ct_select((uint8_t)(i == idx), sbox[i], out);
    return out;
}
