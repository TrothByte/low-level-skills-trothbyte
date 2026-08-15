// GOOD: APICv / posted-interrupt setup — PID address, NV and NDST all
// programmed, then verified.
//
// Path: physical IRQ -> PID (posted via NDST+NV) -> notification vector ->
// virtual APIC page -> guest vector. Every element must be valid or the
// interrupt is silently dropped. Bad version: examples/bad/bad_apicv_pid.c.
//
// Target toolchain: real APICv-capable host. Documentary here.

#include <stdint.h>

#define VMCS_POSTED_INTR_DESC   0x2016
#define VMCS_SECONDARY_CTLS    0x401E
#define SEC_VIRT_APIC_ACCESS   (1ULL << 8)
#define SEC_VIRT_INT_DELIVERY  (1ULL << 9)
#define POSTED_NOTIF_VECTOR     0xE0   /* must be a valid vector */

typedef struct {
    uint16_t nv;
    uint8_t  ndst;
    uint8_t  reserved[29];
    uint32_t pir[8];
} posted_intr_desc_t;

static uint64_t rdmsr(uint32_t msr) { (void)msr; return 0; }
static void vmwrite(uint64_t f, uint64_t v) { (void)f; (void)v; }
extern uint64_t host_phys_of(const void *p);

int setup_apicv(posted_intr_desc_t *pid, uint8_t vcpu_apic_id)
{
    uint64_t proc2 = rdmsr(0x482);   /* IA32_VMX_PROCBASED_CTLS2 */

    /* 1. Configure the PID before enabling virtual-interrupt delivery. */
    pid->nv   = POSTED_NOTIF_VECTOR;
    pid->ndst = vcpu_apic_id;
    vmwrite(VMCS_POSTED_INTR_DESC, host_phys_of(pid));  /* host physical address */

    /* 2. Enable APIC virtualization + virtual-interrupt delivery. */
    proc2 |= SEC_VIRT_APIC_ACCESS | SEC_VIRT_INT_DELIVERY;
    vmwrite(VMCS_SECONDARY_CTLS, proc2);

    /* 3. Sanity: the PID must be resident in a real host frame. */
    if (host_phys_of(pid) == 0) return -1;
    return 0;
}
