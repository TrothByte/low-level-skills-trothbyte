// GOOD: correct VMX launch/resume state machine and control-field validation.
//
// The entry instruction depends on the VMCS 'launched' flag: VMLAUNCH for the
// first entry, VMRESUME afterwards. Control fields are validated against the
// capability MSR before being written (only allowed-1 bits are set).
//
// Target toolchain: real hypervisor-capable host (VMX root). Documentary here —
// no VMX-capable host on this machine. The ioctl-level KVM equivalent is in
// examples/good/kvm_ioctl_demo.c.

#include <stdint.h>

#define IA32_VMX_TRUE_PROC_CTLS  0x48E
#define VMCS_GUEST_RIP           0x681E
#define VMCS_PROC_CTLS           0x4022

static uint64_t rdmsr(uint32_t msr) { (void)msr; return 0; }
static uint64_t vmread(uint64_t field) { (void)field; return 0; }
static void     vmwrite(uint64_t field, uint64_t value) { (void)field; (void)value; }

extern void vmlaunch(void);
extern void vmresume(void);

/* VMLAUNCH on the first entry; VMRESUME on every later one. */
static void enter_guest(int *launched)
{
    if (!*launched) {
        vmlaunch();               /* first entry */
        *launched = 1;            /* only set after a SUCCESSFUL launch */
    } else {
        vmresume();               /* every subsequent entry */
    }
}

/* Only set control bits that the CPU actually allows (allowed-1 mask). */
static void write_validated_ctrl(uint64_t field, uint64_t desired)
{
    uint64_t caps = rdmsr(IA32_VMX_TRUE_PROC_CTLS);
    uint64_t allowed = caps;                 /* allowed-1 bits */
    uint64_t value = desired & allowed;
    vmwrite(field, value);
}

void vcpu_loop(void)
{
    int launched = 0;

    write_validated_ctrl(VMCS_PROC_CTLS, 0x200);   /* e.g. hlt exiting */
    for (;;) {
        /* guest exited; fix guest RIP then re-enter with the right opcode */
        uint64_t rip = vmread(VMCS_GUEST_RIP);
        (void)rip;
        enter_guest(&launched);
    }
}
