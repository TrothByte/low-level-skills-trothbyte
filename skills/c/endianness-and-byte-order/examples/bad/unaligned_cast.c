/*
 * unaligned_cast.c — DEMONSTRATION OF UB THAT HAPPENS TO WORK ON x86.
 *
 * `*(const uint32_t *)p` on an address that is not aligned to the type's
 * alignment is undefined behavior. x86-64 tolerates it in practice
 * (this is why the run below succeeds), but ARM pre-v7 and MIPS raise a
 * data-abort / alignment trap, and CVE-prone parsers have shipped this
 * pattern. Also violates strict aliasing when p aliases char data, and
 * gives endianness-dependent values.
 *
 * Compile: gcc -Wall -Wextra -Werror -O2 unaligned_cast.c
 * Try:     gcc -fsanitize=undefined -O2 unaligned_cast.c
 * UBSan reports: "runtime error: load of misaligned address".
 * The portable alternative is memcpy into a properly aligned uint32_t.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Big-endian byte stream as it would arrive from the wire. The uint16
 * value at offset 1 is deliberately misaligned (odd address). */
static const unsigned char wire[8] = {
    0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE, 0xF0
};

int main(void) {
    const uint16_t *unaligned = (const uint16_t *)(wire + 1);

    /* UB: misaligned access + strict-aliasing violation + host byte order.
     * Works on x86; traps on strict-alignment targets. */
    printf("unaligned cast: 0x%04x (works on x86, UB: misaligned load)\n",
           (unsigned)*unaligned);

    /* Well-defined equivalent: memcpy does not require alignment and is
     * the portable way to assemble multi-byte fields from a byte stream. */
    uint16_t ok;
    memcpy(&ok, wire + 1, sizeof ok);
    printf("memcpy equivalent: 0x%04x (host byte order)\n", (unsigned)ok);
    return 0;
}
