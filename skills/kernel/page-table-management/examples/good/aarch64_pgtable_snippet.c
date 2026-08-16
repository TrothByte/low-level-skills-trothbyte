// GOOD: AArch64 VMSAv8-64 4-KiB-granule page-table walk constants. Compiles
// with the Arm target toolchain; on this host use clang/gnu cross as noted in
// SKILL.md. 48-bit VA with T0SZ=16: levels 0-3, 9 bits per level.
#include <stdint.h>

typedef uint64_t pte_t;

// descriptor bit layout (Arm ARM VMSAv8-64, table D5-10/D5-11)
#define PTE_VALID  (1ULL << 0)
#define PTE_TABLE  (1ULL << 1)   // level 0/1/2: table descriptor bit 1
#define PTE_PAGE   (1ULL << 1)   // level 3: page descriptor
#define PTE_AF     (1ULL << 10)  // access flag
#define PTE_UXN    (1ULL << 54)  // unprivileged execute-never
#define PTE_PXN    (1ULL << 53)  // privileged execute-never
#define PTE_AP_RO  (1ULL << 7)   // AP[1] = read-only

// level index: (va >> (12 + 9 * (3 - level))) & 0x1ff, level 3 at shift 12
static uint64_t va_to_index(uint64_t va, unsigned level) {
    return (va >> (12u + 9u * (3u - level))) & 0x1ffu;
}

// GOOD: a correctly-constructed stage-1 translation table root descriptor.
static pte_t make_table_descriptor(uint64_t next_level_pa) {
    return PTE_VALID | PTE_TABLE | (next_level_pa & 0xfffffffff000ULL);
}

uint64_t walk_indices(uint64_t va) {
    uint64_t idx = 0;
    for (unsigned lvl = 0; lvl < 4; ++lvl)
        idx = idx * 512u + va_to_index(va, lvl);
    return idx;
}

// referenced here so the descriptor helpers are not "unused" in a build
// that only includes this file; real consumers call them directly.
static pte_t __attribute__((used)) touch(void) {
    return make_table_descriptor(0x1000) | PTE_AF;
}
