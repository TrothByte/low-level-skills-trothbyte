// GOOD: SVM exit-code decoding per the AMD manual (not Intel VMX values).
//
// SVM reports the exit reason in VMCB.EXITCODE, with intercept details in
// EXITINFO1/EXITINFO2. E.g. NPF (nested page fault) is EXITCODE 0x400 with the
// faulting guest address in EXITINFO2. Handling must switch on AMD codes and
// never blindly resume an unknown exit (exit storm).
//
// Target toolchain: real SVM-capable host. Documentary here.

#include <stdint.h>

#define SVM_EXIT_NPF         0x400
#define SVM_EXIT_VMRUN       0x40
#define SVM_EXIT_CR0_READ    0x410
#define SVM_EXIT_INTR        0x60

typedef struct {
    uint64_t exitcode;    /* VMCB.EXITCODE */
    uint64_t exitinfo1;   /* per-intercept meaning */
    uint64_t exitinfo2;   /* e.g. faulting GPA on NPF */
    uint64_t guest_rip;
} vmcb_saved_t;

typedef enum { HANDLED, RETRY, STOP } exit_result_t;

extern void install_npt_mapping(uint64_t gpa, uint64_t hpa);
extern void inject_external_intr(void);
extern void emulate_cr0_read(void);

exit_result_t vmexit_handler_svm(vmcb_saved_t *v)
{
    switch (v->exitcode) {
    case SVM_EXIT_NPF:
        /* NPT walk failed for guest PA v->exitinfo2: allocate host frame and
           install an NPT leaf gpa->hpa, then RETRY (resume the guest). */
        install_npt_mapping(v->exitinfo2, /* hpa */ 0x5000);
        return RETRY;
    case SVM_EXIT_INTR:
        inject_external_intr();
        return HANDLED;
    case SVM_EXIT_CR0_READ:
        emulate_cr0_read();
        return HANDLED;
    default:
        /* Unknown reason: log and stop, NEVER resume blindly. */
        return STOP;
    }
}
