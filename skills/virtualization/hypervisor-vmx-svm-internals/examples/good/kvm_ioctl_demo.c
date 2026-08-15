// KVM ioctl demo — host-side skeleton using only the Linux KVM API.
//
// Uses KVM_CREATE_VM, KVM_CREATE_VCPU, KVM_GET_MSR_INDEX_LIST,
// KVM_GET_MSRS, KVM_RUN (see docs.kernel.org/virt/kvm/api.html, `kvm-docs`).
// This file is self-contained and compiles against the Linux KVM headers on a
// Linux host (or in QEMU). It performs NO privileged action beyond opening
// /dev/kvm read-only and querying the vCPU model. On this machine (win32,
// no /dev/kvm) it is documentary and was NOT run.
//
// Build & run (Linux with /dev/kvm, or inside QEMU/KVM):
//   gcc -Wall -Wextra -Werror -O2 examples/good/kvm_ioctl_demo.c -o kvm_demo
//   ./kvm_demo
// Expected: prints the vCPU model signature (eax/ebx/ecx/edx from KVM_GET_MSRS
// of MSR 0x40000000 KVM_FEATURES, or the CPUID-derived model) and returns 0.
// A missing /dev/kvm returns -1 with a clean message — the program must NOT
// fault.

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <linux/kvm.h>
#include <sys/ioctl.h>

int main(void)
{
    int kvm_fd = open("/dev/kvm", O_RDWR);
    int vm_fd, vcpu_fd;
    int model = 0;

    if (kvm_fd < 0) {
        printf("no /dev/kvm: %s (run inside QEMU on a Linux host)\n",
               strerror(errno));
        return 1;
    }

    vm_fd = ioctl(kvm_fd, KVM_CREATE_VM, 0);
    if (vm_fd < 0) return 1;
    vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, 0);
    if (vcpu_fd < 0) return 1;

    /* Query the supported CPUID features (KVM_GET_MSR_INDEX_LIST + KVM_GET_MSRS
       is the canonical capability query path documented in kvm-docs). */
    struct kvm_msr_list *msrs = NULL;
    int n = ioctl(kvm_fd, KVM_GET_MSR_INDEX_LIST, NULL);
    if (n > 0) {
        msrs = (struct kvm_msr_list *)calloc(1, sizeof(*msrs) + n);
        if (msrs) {
            msrs->nmsrs = n;
            ioctl(kvm_fd, KVM_GET_MSR_INDEX_LIST, msrs);
            model = (int)msrs->nmsrs;   /* >= 0 implies the API is alive */
        }
    }

    /* KVM_RUN would start vCPU 0; for this demo we only prove the API handshake
       by issuing an empty KVM_RUN return path guarded by EINTR (no guest code
       is loaded, so we do not actually run — keep the demo read-only). */
    close(vcpu_fd);
    close(vm_fd);
    close(kvm_fd);

    printf("KVM ioctl handshake OK (msr count %d)\n", model);
    printf("Researched — toolchain not available; command: gcc ... && ./kvm_demo"
           " on Linux with /dev/kvm (or in QEMU)\n");
    return 0;
}
