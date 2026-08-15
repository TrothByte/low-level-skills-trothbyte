// intentionally incorrect — BAD example: VMLAUNCH called on an already-launched
// VMCS.
//
// The entry loop uses VMLAUNCH on every iteration. After the first successful
// launch, the VMCS is in "launched" state and only VMRESUME is legal; a
// VMLAUNCH now faults with VMfailValid, VM-instruction error 1 ("VMLAUNCH with
// non-clear VMCS"). The result is a hypervisor that enters once, exits once,
// and then fails every subsequent entry — usually a hang or immediate host
// fault that looks like a guest bug.
//
// Compare: examples/good/good_vmx_launch_resume.c

#include <stdint.h>

#define VMCS_CTRL0              0x4000   /* PIN-BASED_VM_EXEC_CONTROL */
#define VMCS_GUEST_RIP          0x681E

static uint64_t vmread(uint64_t field) { (void)field; return 0; }
static void     vmwrite(uint64_t field, uint64_t value) { (void)field; (void)value; }

extern void vmlaunch(void);   /* asm: vmlaunch */
extern void vmresume(void);   /* asm: vmresume */

static int launched = 0;      /* NEVER updated below — the bug */

void vcpu_loop(void)
{
    for (;;) {
        /* guest just exited; fix guest RIP from the VMCS */
        uint64_t rip = vmread(VMCS_GUEST_RIP);
        (void)rip;

        /* BUG: always VMLAUNCH, even after the first successful entry. */
        vmlaunch();

        /* handle the VM exit reason ... */
        (void)vmread(VMCS_CTRL0);
        launched = 1;         /* set too late: not used by the entry decision */
    }
}
