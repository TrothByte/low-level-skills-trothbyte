/*
 * BAD: // intentionally incorrect — secret value used as an array index.
 * A memory read whose address depends on a secret leaks which element was
 * touched through cache timing (the accessed cache line stays faster). This is
 * the primitive behind cache-timing attacks on S-boxes and key tables.
 */
#include <stddef.h>
#include <stdint.h>

extern const uint8_t sbox[256];

static uint8_t sbox_lookup(const uint8_t *data, size_t len, uint8_t key) {
    uint8_t out = 0;
    for (size_t i = 0; i < len; i++)
        out ^= sbox[data[i] ^ key]; /* index depends on the key */
    return out;
}

uint8_t leaky_transform(const uint8_t *in, size_t len, uint8_t key) {
    return sbox_lookup(in, len, key);
}
