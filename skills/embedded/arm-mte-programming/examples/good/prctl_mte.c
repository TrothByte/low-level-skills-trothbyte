/*
 * prctl_mte.c -- correct ARM MTE enabling sequence.
 *
 * TARGET-ONLY: this file is a sketch for ARMv8.5+/Armv9 Linux (FEAT_MTE).
 * It is NOT compiled here: this host is x86 MinGW (PE/COFF), cannot assemble
 * AArch64 and cannot run MTE. Build on an ARMv9 device, Android NDK, or QEMU:
 *
 *   aarch64-linux-gnu-gcc -mcpu=armv9-a -march=armv9-a+memtag \
 *       -o prctl_mte prctl_mte.c
 *   # or under QEMU user-mode:
 *   qemu-aarch64 -cpu max ./prctl_mte
 *
 * The prctl constants below are the real Linux UAPI values
 * (include/uapi/linux/prctl.h), and the SIGSEGV codes are the kernel's
 * SEGV_MTEAERR/SEGV_MTESERR definitions (asm/sigcontext.h).
 */
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <unistd.h>

/* linux/prctl.h UAPI values (checked against the kernel source). */
#ifndef PR_SET_TAGGED_ADDR_CTRL
#define PR_SET_TAGGED_ADDR_CTRL 55
#define PR_GET_TAGGED_ADDR_CTRL 56
#define PR_TAGGED_ADDR_ENABLE (1UL << 0)
#define PR_MTE_TCF_SHIFT 1
#define PR_MTE_TCF_NONE (0UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TCF_SYNC (1UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TCF_ASYNC (2UL << PR_MTE_TCF_SHIFT)
#define PR_MTE_TAG_MASK_SHIFT 3
#define PR_MTE_TAG_MASK (0xffffUL << PR_MTE_TAG_MASK_SHIFT)
#endif

/* SIGSEGV codes for tag faults (asm/sigcontext.h). */
#ifndef SEGV_MTEAERR
#define SEGV_MTEAERR 35 /* asynchronously detected tag error  */
#define SEGV_MTESERR 36 /* synchronously detected tag error   */
#endif

#define GRANULE_16 16
#define MTE_TAG_SHIFT 56

static void tag_fault_handler(int sig, siginfo_t *si, void *unused)
{
    /* SYNC faults carry the precise faulting address in si_addr. */
    if (si->si_code == SEGV_MTESERR)
        fprintf(stderr, "MTE SYNC tag fault at %p\n", si->si_addr);
    else if (si->si_code == SEGV_MTEAERR)
        fprintf(stderr, "MTE ASYNC tag error reported (faulting address is "
                        "NOT precise -- rerun in SYNC mode to localize)\n");
    else
        fprintf(stderr, "unexpected SIGSEGV code %d\n", si->si_code);
    _exit(128 + sig);
}

/* Check MTE support: the prctl itself is the portable gate. */
static int mte_enable(int tcf)
{
    long r = prctl(PR_SET_TAGGED_ADDR_CTRL,
                   PR_TAGGED_ADDR_ENABLE | (unsigned long)tcf |
                       PR_MTE_TAG_MASK,
                   0, 0, 0);
    if (r != 0) {
        fprintf(stderr, "prctl PR_SET_TAGGED_ADDR_CTRL failed: %s\n",
                strerror(errno));
        return -1;
    }
    return 0;
}

static uint64_t insert_random_tag(void *p)
{
    uint64_t rtag;
    __asm__ volatile("irg %0, %1" : "=r"(rtag) : "r"((uint64_t)p));
    return rtag; /* tag in bits 56-59; TBI makes the address dereferenceable */
}

int main(void)
{
    struct sigaction sa = {0};
    sa.sa_sigaction = tag_fault_handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);

    /* Enable MTE: choose SYNC for debugging (precise), ASYNC for production. */
    if (mte_enable(PR_MTE_TCF_SYNC) != 0) {
        puts("MTE unavailable (unsupported CPU, kernel, or seccomp). "
             "Do NOT tag allocations blindly.");
        return 1;
    }

    /*
     * Allocations MUST be 16-byte aligned and tagged. posix_memalign gives
     * the alignment; IRG inserts a fresh random tag; STG writes the
     * allocation tag for the whole granule.
     */
    void *base = NULL;
    if (posix_memalign(&base, GRANULE_16, 4096) != 0) {
        perror("posix_memalign");
        return 1;
    }
    uint64_t tagged = insert_random_tag(base);
    __asm__ volatile("stg %0, [%1]"
                     :
                     : "r"((uint64_t)(tagged >> MTE_TAG_SHIFT)), "r"(base));
    __asm__ volatile("ldg %0, [%1]"
                     : "=r"(tagged)
                     : "r"(base));

    puts("MTE enabled (SYNC), allocation tagged; press Ctrl-C");
    while (1)
        pause();
    return 0;
}
