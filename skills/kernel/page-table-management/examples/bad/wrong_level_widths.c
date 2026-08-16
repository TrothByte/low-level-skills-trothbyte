// BAD: "x86-64 paging uses 10-bit level indices / PAE bit widths" — treats the
// level fields as 10 bits each (PAE/32-bit style) and drops the PML4 level.
// Wrong indices are silently accepted; the walk is structurally incorrect.
// intentionally incorrect
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t vaddr;

// WRONG: 10-bit fields are a 32-bit/PAE convention; long mode uses 9 bits.
// WRONG: PML4 (top 9 bits) is dropped entirely.
static void walk_wrong_widths(void) {
    unsigned pdpt = (vaddr >> 30) & 0x3ff;  // 10 bits
    unsigned pd   = (vaddr >> 20) & 0x3ff;  // 10 bits
    unsigned pt   = (vaddr >> 10) & 0x3ff;  // 10 bits
    printf("va=%#llx\n", (unsigned long long)vaddr);
    printf("PDPT=%u\n", pdpt);
    printf("PD  =%u\n", pd);
    printf("PT  =%u\n", pt);
    printf("(values are WRONG: no PML4, 10-bit fields)\n");
}

int main(int argc, char **argv) {
    vaddr = argc > 1 ? strtoull(argv[1], NULL, 0) : 0xffff888012345678ULL;
    walk_wrong_widths();
    return 0;
}
