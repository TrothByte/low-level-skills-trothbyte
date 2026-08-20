/*
 * bitfield_hdr.c — DEMONSTRATION OF A NON-PORTABLE BITFIELD RECORD.
 *
 * Bitfields are valid C but their layout is implementation-defined: bit
 * order, padding, allocation order (MSB-first vs LSB-first) depend on the
 * ABI. Two hosts -- same compiler flags, opposite allocation order -- read
 * the field value differently. Never use bitfields for wire/file formats;
 * assemble the bits yourself with shifts.
 *
 * Compile: gcc -Wall -Wextra -Werror -O2 bitfield_hdr.c
 * Run on x86 (little-endian) and on QEMU s390x/ppc64 to see the field
 * value flip.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    unsigned char version : 4;  /* bits 0-3 on LSB-first, 4-7 on MSB-first */
    unsigned char type    : 4;
} bf_hdr;

int main(void) {
    bf_hdr h;
    h.type = 0x0A;
    h.version = 0x3;   /* 0x3A on MSB-first allocation, 0xA3 on LSB-first */

    uint8_t raw;
    memcpy(&raw, &h, 1);   /* punning a single byte: portable trick, see note */
    printf("bitfield hdr: version=0x%x type=0x%x -> raw byte 0x%02x "
           "(implementation-defined bit order)\n",
           h.version, h.type, raw);
    return 0;
}
