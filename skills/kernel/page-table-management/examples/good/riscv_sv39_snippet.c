// GOOD: RISC-V Sv39 page-table walk constants (privileged spec). Sv39: 39-bit
// VA, 3 levels, 9 bits per level, 4 KiB pages. satp.MODE=8 selects Sv39.
#include <stdint.h>

typedef uint64_t pte_t;

// Sv39 PTE layout (RISC-V privileged spec, Sv39 section)
#define PTE_V (1ULL << 0)   // valid
#define PTE_R (1ULL << 1)   // readable
#define PTE_W (1ULL << 2)   // writable
#define PTE_X (1ULL << 3)   // executable
#define PTE_U (1ULL << 4)   // user
#define PTE_G (1ULL << 5)   // global
#define PTE_A (1ULL << 6)   // accessed
#define PTE_D (1ULL << 7)   // dirty
// W^X rule: R and X may both be 1 (readable+executable is legal on RISC-V),
// but a page may never be both W and X.
#define PTE_WX_FORBIDDEN 1  // helper sentinel; see reasoning rules

static uint64_t sv39_index(uint64_t va, unsigned level) {
    // levels: 0 (vpn[2], bits 29:21), 1 (vpn[1], 20:12), 2 (vpn[0], 11:0)
    return (va >> (12u + 9u * level)) & 0x1ffu;
}

// GOOD: a leaf PTE for a 4 KiB user, read-only, non-executable page.
static pte_t make_leaf(uint64_t ppn) {
    return (ppn & 0x3fffffffc000ULL) | PTE_V | PTE_R | PTE_U;
}

int main(void) {
    // 0x80000000 (kernel high-half start in Sv39) → vpn[2]=0, vpn[1]=0, vpn[0]=0
    uint64_t va = 0x80000000ULL;
    // exercise the leaf constructor so it is not flagged unused
    pte_t leaf = make_leaf(0x12345000);
    return (int)(sv39_index(va, 0) + sv39_index(va, 1) + sv39_index(va, 2) +
                 (leaf & PTE_R));
}
