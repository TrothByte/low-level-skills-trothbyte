/*
 * endian_probe.c — runtime endianness detection without UB.
 *
 * Write 0x01020304 into a uint32_t via memcpy (not a union, not a pointer
 * cast), then inspect its first byte. First byte 0x01 => big-endian
 * (MSB at lowest address); 0x04 => little-endian (LSB first).
 *
 * Compile: gcc -Wall -Wextra -Werror -O2 endian_probe.c
 * Cross-check on a big-endian target (QEMU s390x/ppc64) to see the
 * opposite branch fire.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *detect_endianness(void) {
    uint32_t magic = 0x01020304u;      /* chosen: each byte is unique */
    unsigned char bytes[4];
    memcpy(bytes, &magic, sizeof magic);
    if (bytes[0] == 0x01)
        return "big-endian";           /* MSB stored at lowest address */
    if (bytes[0] == 0x04)
        return "little-endian";        /* LSB stored at lowest address */
    return "neither (mixed/unknown)";  /* e.g. PDP-endian, practically dead */
}

int main(void) {
    printf("endian_probe: host is %s\n", detect_endianness());
    return 0;
}
