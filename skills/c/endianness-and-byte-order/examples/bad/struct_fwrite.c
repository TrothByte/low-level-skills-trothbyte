/*
 * struct_fwrite.c — DEMONSTRATION OF A NON-PORTABLE PATTERN (DO NOT COPY).
 *
 * Serializing a native struct with memcpy/fwrite bakes in the host's
 * padding AND byte order. The same bytes are wrong on a big-endian host
 * and on any host with a different struct layout. Run it, look at the
 * output, and remember: this is what "it works on my machine" looks like.
 *
 * Compile: gcc -Wall -Wextra -Werror -O2 struct_fwrite.c
 * This is an intentional anti-pattern; the skill's eval checker must flag
 * "struct fwrite/memcpy to wire".
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* A "network" record. Padding is implementation-defined: on x86-64 SysV
 * this struct is 8 bytes with 1 padding byte after 'tag'. */
typedef struct {
    uint8_t tag;     /* 1 byte   */
    uint16_t kind;   /* 2 bytes  */
    uint32_t value;  /* 4 bytes  */
} net_record;

static void write_naive(FILE *out, const net_record *r) {
    /* Non-portable: writes host padding and host byte order. */
    fwrite(r, sizeof *r, 1, out);
}

int main(void) {
    net_record rec = { 0xAB, 0x1234, 0xDEADBEEFu };

    /* Print the host memory layout of the struct. */
    unsigned char raw[sizeof(net_record)];
    memcpy(raw, &rec, sizeof raw);   /* punning demo: memcpy is fine here */
    printf("struct_fwrite: sizeof = %zu\n", sizeof(net_record));
    printf("  host bytes (little-endian x86):");
    for (size_t i = 0; i < sizeof raw; i++)
        printf(" %02x", raw[i]);
    printf("\n");

    /* Write it to a file, as the non-portable pattern does. */
    FILE *f = fopen("struct_fwrite.bin", "wb");
    if (f) {
        write_naive(f, &rec);
        fclose(f);
    }

    printf("  note: padding at offset %zu and LSB-first field bytes make "
           "this format host-specific\n", offsetof(net_record, kind));
    return 0;
}
