/*
 * union_punning.c — DEMONSTRATION OF UNDEFINED BEHAVIOR (DO NOT COPY).
 *
 * Reading an integer through a union member of a different type (here:
 * uint32_t vs uint8_t[4]) is type punning via the inactive union member,
 * which is undefined behavior in C (it is permitted only in some C++
 * implementations as a vendor extension). The compiler is allowed to
 * assume the inactive member is never read; under optimization the bytes
 * observed can be garbage, and on a big-endian host the byte order is the
 * opposite of what this prints on x86.
 *
 * Compile: gcc -Wall -Wextra -Werror -O2 union_punning.c
 * gcc may warn with -Wstrict-aliasing at higher -O levels.
 * The correct, well-defined way to inspect bytes is memcpy (see
 * examples/good/portable_serialize.c).
 */
#include <stdint.h>
#include <stdio.h>

typedef union {
    uint32_t u;
    uint8_t  b[4];   /* inactive member when reading after writing u */
} pun_u;

int main(void) {
    pun_u x;
    x.u = 0x01020304u;

    /* Reading x.b[] after writing x.u reads the inactive union member:
     * UB. Result is endianness-dependent, so a big-endian build flips
     * the byte order. */
    printf("union punning: 0x01020304 as bytes -> %02x %02x %02x %02x "
           "(host byte order; UB)\n", x.b[0], x.b[1], x.b[2], x.b[3]);
    return 0;
}
