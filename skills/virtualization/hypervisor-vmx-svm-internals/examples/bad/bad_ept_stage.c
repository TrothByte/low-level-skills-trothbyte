// intentionally incorrect — BAD example: EPT/NPT stage confusion.
//
// The EPT walk is a two-stage translation: guest VA -> guest PA (guest page
// tables), then guest PA -> host PA (EPT/NPT host tables). The EPT leaf entry
// must contain the HOST physical frame number. This code stores the guest
// physical address directly into the EPT leaf, so the CPU will translate guest
// PA 0x1000 to host PA 0x1000 — whatever host frame happens to be at that
// address — instead of the frame the hypervisor actually allocated. Guest data
// corruption, not a clean fault.
//
// Compare: examples/good/good_ept_map.c

#include <stdint.h>

#define EPT_PAGE_PRESENT 0x001ULL
#define EPT_PAGE_RW      0x002ULL

typedef uint64_t ept_pte_t;

/* BUG: leaf = guest physical address, not the host physical frame number. */
void ept_map_bad(ept_pte_t *ept_pt, uint64_t gpa)
{
    ept_pt[gpa >> 12] = gpa | EPT_PAGE_PRESENT | EPT_PAGE_RW;
}

uint64_t guest_phys_alloc(uint64_t size) { (void)size; return 0x1000; }

int main(void)
{
    ept_pte_t ept[512] = {0};
    uint64_t gpa = guest_phys_alloc(0x1000);   /* guest thinks it owns 0x1000 */

    /* The EPT leaf should map gpa -> host_phys_alloc(page).
       Instead it maps gpa -> gpa (the same number). Stage confusion. */
    ept_map_bad(ept, gpa);
    return 0;
}
