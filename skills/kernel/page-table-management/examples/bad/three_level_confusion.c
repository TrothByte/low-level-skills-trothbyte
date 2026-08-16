// BAD: "3-level x86-64 paging" misconception — treats PML4 as a non-existent
// top level and computes indices as if PAE/32-bit (PDPT+PD+PT only). This
// produces wrong indices and silently accepts a non-canonical VA.
// intentionally incorrect
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static uint64_t vaddr;

// WRONG: only 3 levels; the top 9 bits of a 48-bit VA are dropped.
static void walk_3level(void) {
    unsigned pdpt = (vaddr >> 30) & 0x1ff;
    unsigned pd   = (vaddr >> 21) & 0x1ff;
    unsigned pt   = (vaddr >> 12) & 0x1ff;
    printf("va=%#llx\n", (unsigned long long)vaddr);
    printf("PDPT=%u (wrong: PML4 index ignored)\n", pdpt);
    printf("PD  =%u\n", pd);
    printf("PT  =%u\n", pt);
    printf("accepted non-canonical VA without validation\n");
}

int main(int argc, char **argv) {
    vaddr = argc > 1 ? strtoull(argv[1], NULL, 0) : 0xffff888012345678ULL;
    // WRONG: no canonical check — 0x1_0000_0000_0000 must be rejected.
    if ((vaddr >> 48) != 0xffff && (vaddr >> 47) != 0) {
        printf("error: non-canonical VA\n");
        return 1;
    }
    walk_3level();
    return 0;
}
