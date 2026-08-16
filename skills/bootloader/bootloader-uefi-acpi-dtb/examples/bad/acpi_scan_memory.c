/*
 * BAD: // intentionally incorrect — scans physical memory for the RSDP
 * signature ("RSD PTR ") from a UEFI loader. This is legacy-BIOS behavior:
 * under UEFI the canonical path is the EFI System Table ConfigurationTable
 * matched against the ACPI 2.0 / ACPI 1.0 GUIDs. Memory scanning reads
 * regions the firmware owns and may fault or find stale tables.
 *
 * Build: gcc -Wall -Wextra -Werror -O2 acpi_scan_memory.c -o acpiscan
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* Simulated physical scan region (legacy BIOS trick). */
static uint8_t fake_low_memory[0x20000];

static const uint8_t *scan_for_rsdp(void) {
    /* // intentionally incorrect: BIOS-era EBDA/high-memory scan */
    for (size_t off = 0; off < 0x20000; off += 16) {
        if (memcmp(fake_low_memory + off, "RSD PTR ", 8) == 0)
            return fake_low_memory + off;
    }
    return NULL;
}

int main(void) {
    (void)scan_for_rsdp;  /* referenced; the bug is the strategy itself */
    printf("BUG: scanning memory instead of using UEFI ConfigurationTable\n");
    return 0;
}
