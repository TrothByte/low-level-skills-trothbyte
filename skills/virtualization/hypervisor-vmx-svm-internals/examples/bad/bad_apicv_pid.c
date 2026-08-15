// intentionally incorrect — BAD example: APICv / posted-interrupt PID
// misconfiguration.
//
// Enabling "virtual interrupt delivery" (SECONDARY_PROC_CTLS bit 9) without a
// valid posted-interrupt descriptor address silently drops every interrupt: the
// PID address (VMCS 0x2016, posted_intr_desc_addr) must be a valid host physical
// address whose NV field holds a usable notification vector and NDST the
// destination vCPU. Here the PID address is left 0 and NV is never set, so
// interrupts meant for the guest disappear entirely — a subtle deadlock that
// looks like a guest bug, not a hypervisor config error.
//
// Compare: examples/good/good_apicv.c

#include <stdint.h>

#define VMCS_PIN_CTLS            0x4000
#define VMCS_SECONDARY_CTLS     0x401E
#define VMCS_POSTED_INTR_DESC   0x2016

#define SEC_VIRT_INT_DELIVERY   (1ULL << 9)
#define SEC_VIRT_APIC_ACCESS    (1ULL << 8)

typedef struct {
    uint16_t nv;          /* notification vector */
    uint8_t  ndst;        /* destination vCPU */
    uint8_t  reserved[29];
    uint32_t pir[8];      /* posted-interrupt requests */
} posted_intr_desc_t;

static uint64_t rdmsr(uint32_t msr) { (void)msr; return 0; }
static void vmwrite(uint64_t f, uint64_t v) { (void)f; (void)v; }

void setup_apicv_bad(void)
{
    uint64_t proc2 = rdmsr(0x482);   /* IA32_VMX_PROCBASED_CTLS2 */

    /* BUG 1: enables virtual interrupt delivery ... */
    proc2 |= SEC_VIRT_APIC_ACCESS | SEC_VIRT_INT_DELIVERY;
    vmwrite(VMCS_SECONDARY_CTLS, proc2);

    /* BUG 2: ... but never programs the posted-interrupt descriptor:
       VMCS_POSTED_INTR_DESC stays 0, so hardware has no valid PID to read.
       Interrupts are dropped. */
    /* (missing: vmwrite(VMCS_POSTED_INTR_DESC, (uint64_t)&pid);) */

    /* BUG 3: even if the address were set, nv would still be 0. */
    (void)VMCS_PIN_CTLS;
}
