/*
 * BAD: // intentionally incorrect — accepts an ACPI table whose header
 * checksum is wrong. The loader parses offsets and trusts the pointer
 * without summing the bytes; a single flipped byte yields fields the OS
 * trusts and crashes on. The modulo-256 checksum is mandatory validation.
 *
 * Build: gcc -Wall -Wextra -Werror -O2 acpi_bad_checksum.c -o acpibad
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;
} __attribute__((packed)) rsdp_legacy_t;

/* No sum_bytes helper, no checksum check — the whole bug in one function. */
static int accept_table(const uint8_t *table, size_t len) {
    (void)table;
    (void)len;
    /* Checksum validation omitted: // intentionally incorrect */
    return 1; /* "valid" */
}

int main(void) {
    uint8_t rsdp[20] = {0};
    memcpy(rsdp, "RSD PTR ", 8);
    rsdp[15] = 2;
    rsdp[12] ^= 0x01;             /* corrupted payload byte */

    if (accept_table(rsdp, sizeof rsdp)) {
        printf("BUG: corrupted RSDP accepted without checksum check\n");
        return 0;                 /* silent acceptance — the failure */
    }
    printf("PASS: rejected\n");
    return 0;
}
