/*
 * bad_async_precision.c -- WRONG: relying on MTE_ASYNC for precise fault
 * attribution.
 *
 * TARGET-ONLY, annotated for review. NOT compiled here.
 *
 * What is wrong:
 *   - In MTE_ASYNC the tag check is done by the memory system in the
 *     background. The faulting instruction completes, and the kernel delivers
 *     SIGSEGV with SEGV_MTEAERR LATER (the PC and si_addr in the handler do
 *     NOT point at the actual bug).
 *   - Debugging an ASYNC report as if it were precise leads to "fixing" the
 *     wrong line, exactly the trap in "What the agent often gets wrong".
 *
 * Correct fix: use PR_MTE_TCF_SYNC while debugging to get the precise
 * faulting instruction, then switch to PR_MTE_TCF_ASYNC for production
 * overhead; treat ASYNC reports only as "a tag fault happened somewhere
 * recently".
 */
#define _GNU_SOURCE
#include <signal.h>
#include <stdio.h>
#include <sys/prctl.h>

#ifndef SEGV_MTEAERR
#define SEGV_MTEAERR 35
#endif

static void handler(int sig, siginfo_t *si, void *ctx)
{
    if (si->si_code == SEGV_MTEAERR)
        fprintf(stderr, "fault at %p\n", si->si_addr); /* WRONG assumption:
        this address is only a recent-tag-fault hint, not the bug site */
    /* ...code "fixing" si->si_addr follows... */
}

void bad_setup(void)
{
    struct sigaction sa = {0};
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);

    /* WRONG pattern: hand-rolled bit literals. The correct way is the named
     * constants PR_TAGGED_ADDR_ENABLE | PR_MTE_TCF_ASYNC | PR_MTE_TAG_MASK;
     * getting TCF bits wrong silently selects NONE (unchecked) or the wrong
     * mode. */
    prctl(PR_SET_TAGGED_ADDR_CTRL, 1UL | (2UL << 1) | (0xffffUL << 3), 0, 0, 0);
}
