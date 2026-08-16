/*
 * GOOD: ACPI RSDP + table header checksum validation (host-runnable).
 * Every ACPI table (RSDP, XSDT, and every descriptor) carries a checksum
 * such that the sum of all bytes of the structure is 0 modulo 256. A
 * loader must validate each hop before trusting the pointer.
 *
 * Build: gcc -Wall -Wextra -Werror -O2 acpi_checksum.c -o acpichk
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Minimal ACPI structs: only the bytes the checksum math needs. */
typedef struct {
    char     signature[8];
    uint8_t  checksum;
    char     oem_id[6];
    uint8_t  revision;
    uint32_t rsdt_address;   /* 32-bit RSDT address (ACPI 1.0) */
} __attribute__((packed)) rsdp_legacy_t;

typedef struct {
    uint64_t x;
} __attribute__((packed)) xsdt_entry_t;

static uint8_t sum_bytes(const uint8_t *p, size_t n) {
    uint8_t s = 0;
    for (size_t i = 0; i < n; i++)
        s = (uint8_t)(s + p[i]);
    return s;
}

/* Build an RSDP whose checksum makes the whole struct sum to 0 (mod 256). */
static void fix_checksum(uint8_t *p, size_t n) {
    p[8] = (uint8_t)(0 - sum_bytes(p, n));
}

int main(void) {
    /* Valid RSDP: signature "RSD PTR ", checksum field at offset 8. */
    uint8_t rsdp[20] = {0};
    memcpy(rsdp, "RSD PTR ", 8);
    rsdp[15] = 2;                 /* ACPI revision 2 -> XSDT expected */
    fix_checksum(rsdp, sizeof rsdp);
    assert(sum_bytes(rsdp, sizeof rsdp) == 0);

    /* Corrupt one byte (flip a payload bit, do NOT touch checksum). */
    rsdp[12] ^= 0x01;
    if (sum_bytes(rsdp, sizeof rsdp) == 0) {
        printf("FAIL: corrupt table accepted\n");
        return 1;
    }
    printf("PASS: RSDP checksum math rejects corruption\n");

    /* XSDT-style table header checksum (same modulo-256 rule). */
    uint8_t hdr[36] = {0};
    memcpy(hdr, "XSDT", 4);
    hdr[8] = 0;                   /* checksum placeholder */
    fix_checksum(hdr, sizeof hdr);
    assert(sum_bytes(hdr, sizeof hdr) == 0);
    hdr[4] = 36;                  /* length field */
    if (sum_bytes(hdr, sizeof hdr) == 0) {
        printf("FAIL: length edit not caught\n");
        return 1;
    }
    printf("PASS: table header checksum validation\n");
    return 0;
}
