/* pci_structs.c — BUGGY host model of PCIe config space (agent-bug class).
 *
 * Deliberate defects that the skill's reasoning rejects:
 *   1. Capabilities read at FIXED offsets (0x40, 0x50, 0x60, 0x70) instead of
 *      following next pointers — a differently-laid-out device is misparsed.
 *   2. No cycle/termination guard on the walk (infinite loop if next != 0).
 *   3. BAR probe masks width bits 0-15 instead of 0-3, yielding wrong sizes.
 *   4. 64-bit BAR type (0x3) treated as 32-bit.
 *   5. MSI-X alignment (8-byte entries, page-aligned table) ignored.
 *   6. MSI-X table offset read from the wrong BAR (BIR ignored).
 *
 * Compile (clean, -Werror — bugs are semantic, not syntactic):
 *   gcc -Wall -Wextra -Werror -O2 examples/bad/pci_structs.c -o bad.exe
 * Run: bad.exe   (prints WRONG values; must be rejected by review)
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* FIXED cap offsets — wrong: only the next-pointer chain is authoritative */
static const uint8_t FIXED_CAP_OFFS[] = {0x40, 0x50, 0x60, 0x70, 0x00};

static const char *cap_name(uint8_t id) {
    switch (id) {
        case 0x01: return "PM";
        case 0x05: return "MSI";
        case 0x10: return "PCIe";
        case 0x11: return "MSI-X";
        default:   return "?";
    }
}

/* Wrong BAR mask: clears bits 0-15 (space+type+size low bits) instead of 0-3 */
static uint32_t probe_32_mem_wrong(uint32_t rb) {
    return ((~rb) & 0xFFFF0000u) + 1u;
}

/* 64-bit BAR treated as 32-bit — size truncated */
static uint32_t probe_32_mem_wrong64(uint32_t rb) {
    return ((~rb) & 0xFFFFFFFFu & ~0x0Fu) + 1u;
}

int main(void) {
    uint8_t cfg[256];
    memset(cfg, 0, sizeof cfg);
    cfg[0x0E] = 0x00;
    cfg[0x00] = 0x86; cfg[0x01] = 0x80;
    cfg[0x02] = 0x0E; cfg[0x03] = 0x10;
    cfg[0x09] = 0x00; cfg[0x0A] = 0x00; cfg[0x0B] = 0x02;
    cfg[0x10] = 0x00; cfg[0x11] = 0x00; cfg[0x12] = 0x00; cfg[0x13] = 0xE0;
    cfg[0x34] = 0x40;
    cfg[0x40] = 0x01; cfg[0x41] = 0x50;
    cfg[0x50] = 0x05; cfg[0x51] = 0x60;
    cfg[0x52] = 0x80; cfg[0x53] = 0x00;
    cfg[0x54] = 0x00; cfg[0x55] = 0x00; cfg[0x56] = 0xE0; cfg[0x57] = 0xFE;
    cfg[0x58] = 0x00; cfg[0x59] = 0x00; cfg[0x5A] = 0x00; cfg[0x5B] = 0x00;
    cfg[0x5C] = 0x33; cfg[0x5D] = 0x00;
    cfg[0x60] = 0x10; cfg[0x61] = 0x70;
    cfg[0x70] = 0x11; cfg[0x71] = 0x00;
    cfg[0x72] = 0x03; cfg[0x73] = 0x00;
    cfg[0x74] = 0x00; cfg[0x75] = 0x10; cfg[0x76] = 0x00; cfg[0x77] = 0x00;
    cfg[0x78] = 0x00; cfg[0x79] = 0x20; cfg[0x7A] = 0x00; cfg[0x7B] = 0x00;

    printf("== PCIe config space model (BAD — for review) ==\n");

    /* 1. Fixed-offset capability dump: fails to follow the chain.
     *    Here the fixed offsets happen to match, but a device whose PM cap is
     *    at 0x44 (legal per spec) would print garbage. */
    printf("caps (fixed offsets):");
    for (int i = 0; FIXED_CAP_OFFS[i] != 0; i++) {
        uint8_t off = FIXED_CAP_OFFS[i];
        printf(" 0x%02X=%s", off, cap_name(cfg[off]));
    }
    printf("\n");

    /* 2. Walk without termination guard — the actual next-pointer chain. */
    printf("walk: ");
    uint8_t off = cfg[0x34];
    while (off != 0) {  /* BUG: no next==0 guard, no cycle detection */
        printf("0x%02X=%s ", off, cap_name(cfg[off]));
        off = cfg[off + 1];
    }
    printf("\n");

    /* 3. Wrong BAR mask: 16 MiB -> writes 0xFFFFFFFF, reads 0xFF000000,
     *    wrong mask 0xFFFF0000 -> size 0x00010000 (64 KiB) instead of 16 MiB. */
    printf("BAR0 size (wrong mask): 0x%X\n", probe_32_mem_wrong(0xFF000000u));

    /* 4. 64-bit BAR treated as 32-bit: type bits say 0x3 (64-bit), the low
     *    dword alone gives size 0x40000000 (1 GiB) but the real size is
     *    0x40000000 too here — worse when the size spans both dwords. */
    uint32_t v = 0x00000006u;  /* 64-bit type */
    printf("BAR type: %s (BUG: 64-bit type read as 32-bit)\n",
           (v & 1u) ? "I/O" : "memory 32-bit");
    printf("64-bit BAR size (32-bit truncation): 0x%X\n",
           probe_32_mem_wrong64(0xC0000006u));

    /* 5+6. MSI-X alignment and BIR ignored. */
    uint32_t table = cfg[0x74] | (cfg[0x75] << 8) | (cfg[0x76] << 16)
                     | ((uint32_t)cfg[0x77] << 24);
    uint32_t bir = table & 0x7u;
    uint32_t off8 = table & 0xFFFFF8u;
    printf("MSI-X: BIR=%u table_off=0x%04X (aligned checks skipped)\n",
           bir, off8);

    /* A page-UNaligned table offset that the buggy model accepts (it only
     * checks 8-byte alignment, never page alignment of the table region). */
    uint32_t bad_tab = 0x1008u;  /* 8B aligned but not page aligned */
    printf("MSI-X table 0x%04X page check: %s (BUG: page alignment "
           "not enforced)\n", bad_tab,
           (bad_tab & 0xFFFu) ? "would pass" : "pass");

    printf("NOTE: compiles clean under -Werror; all values above must be "
           "rejected by capability-walk + mask + alignment reasoning\n");
    return 0;
}
