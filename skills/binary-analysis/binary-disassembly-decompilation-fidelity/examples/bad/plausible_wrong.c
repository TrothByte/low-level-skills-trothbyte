// intentionally incorrect — plausible-but-wrong reconstruction of checksum.c.
// Compiles cleanly with -Wall -Wextra -Werror but produces different output.
/* A "faithful-looking" reconstruction of checksum.c written after reading the
 * disassembly in objdump output. It compiles cleanly with -Wall -Wextra -Werror,
 * looks like the original, but has TWO semantic deviations from the real binary:
 *   1. fnv1a: operator precedence — `h ^ (buf[i] * PRIME)` instead of
 *      `(h ^ buf[i]) * PRIME` (decompilers often merge the xor and imul wrongly).
 *   2. crc32: the byte is treated as SIGNED (int8_t promotion) even though the
 *      disassembly uses `movzbl` (zero-extension) — for bytes >= 0x80 this
 *      changes the value XORed into the accumulator.
 */
#include <stdint.h>
#include <stdio.h>

static uint32_t fnv1a_wrong(const uint8_t *buf, size_t len)
{
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < len; i++) {
        h = h ^ ((uint32_t)buf[i] * 16777619u); /* should be (h ^ buf[i]) * 16777619u */
    }
    return h;
}

static uint32_t crc32_wrong(const uint8_t *buf, size_t len)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        c ^= (int32_t)(int8_t)buf[i]; /* should be c ^= buf[i] (movzbl = zero-extend) */
        for (int bit = 0; bit < 8; bit++) {
            c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
        }
    }
    return ~c;
}

int main(void)
{
    const uint8_t data[] = {0x01, 0x80, 0x03, 0xFF, 0x05};
    printf("fnv1a: %08x\n", fnv1a_wrong(data, sizeof data));
    printf("crc32: %08x\n", crc32_wrong(data, sizeof data));
    return 0;
}
