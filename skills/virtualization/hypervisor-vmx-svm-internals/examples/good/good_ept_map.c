// GOOD: EPT (stage-2) mapping that separates guest PA from host PA.
//
// The guest address space is translated in two stages:
//   stage 1: guest VA  -> guest PA   (guest's own page tables, walked by the CPU)
//   stage 2: guest PA  -> host PA    (EPT/NPT tables owned by the hypervisor)
// The EPT leaf entry therefore contains a HOST physical frame number. The bad
// version in examples/bad/bad_ept_stage.c wrote the guest PA directly into the
// leaf, corrupting the mapping.
//
// Target toolchain: real KVM/VMM host (EPT-capable). Documentary here — no
// /dev/kvm on this machine. ioctl-level demo: examples/good/kvm_ioctl_demo.c.

#include <stdint.h>

#define EPT_PAGE_PRESENT 0x001ULL
#define EPT_PAGE_RW      0x002ULL
#define EPT_PAGE_USER    0x004ULL

typedef uint64_t ept_pte_t;

/* Simulated host-side frame allocator (the actual allocation happens in the
   VMM's memory manager; here we just prove the leaf stores hpa, not gpa). */
extern uint64_t host_phys_alloc(void *page);

void ept_map(ept_pte_t *ept_pt, uint64_t gpa, void *page)
{
    uint64_t hpa = host_phys_alloc(page);   /* host physical frame number */
    ept_pt[gpa >> 12] = hpa | EPT_PAGE_PRESENT | EPT_PAGE_RW | EPT_PAGE_USER;
}

/* KVM-style helper for reference: KVM_EVENT type comments only — no Linux
   headers on this host, so we keep the ioctl demo self-contained. */
typedef struct {
    uint64_t gpa;
    uint64_t hpa;
    int      flags;
} ept_entry_t;

int main(void)
{
    ept_pte_t ept[512] = {0};
    uint64_t gpa = 0x1000;
    void *page = 0;   /* placeholder host frame */

    ept_map(ept, gpa, page);   /* leaf = hpa | flags, NOT gpa */
    return 0;
}
