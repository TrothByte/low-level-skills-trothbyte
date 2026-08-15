// intentionally incorrect — BAD example: SVM exit code misdecoded with an Intel
// (VMX) exit-reason value.
//
// SVM records the exit reason in VMCB.EXITCODE, and intercepts have a different
// numeric space from Intel's VMX exit reasons. Here the handler tests EXITCODE
// against 48 — the VMX "EPT violation" value — on an SVM VMCB, where 48 is a
// different intercept (or an unknown value). The real NPF (nested page fault)
// reason on AMD is in the 0x400+ range via the VMEXIT_NPF code, and the guest
// address is in EXITINFO2, not in a VMCS field. This handler never fires on the
// actual fault and the exit is treated as unknown.
//
// Compare: examples/good/good_svm_exitcode.c

#include <stdint.h>

#define VMX_EXIT_EPT_VIOLATION 48   /* Intel value — WRONG for SVM */

typedef struct {
    uint64_t exitcode;   /* VMCB.EXITCODE */
    uint64_t exitinfo1;
    uint64_t exitinfo2;
    uint64_t guest_rip;
} vmcb_saved_t;

extern void handle_npf(vmcb_saved_t *v);
extern void forward_guest_fault(vmcb_saved_t *v);

/* BUG: compares an AMD EXITCODE against an Intel exit-reason constant. */
void vmexit_handler(vmcb_saved_t *v)
{
    if (v->exitcode == VMX_EXIT_EPT_VIOLATION) {
        handle_npf(v);
    } else {
        /* NPF on SVM never matches 48 -> falls through as unknown, then
           resumes into a still-faulting guest -> exit storm. */
        forward_guest_fault(v);
    }
}
