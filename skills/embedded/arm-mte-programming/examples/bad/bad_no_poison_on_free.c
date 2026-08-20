/*
 * bad_no_poison_on_free.c -- WRONG: freeing without re-tagging the granule.
 *
 * TARGET-ONLY, annotated for review. NOT compiled here.
 *
 * What is wrong:
 *   - The use-after-free guarantee of MTE comes from the FREE path: the
 *     allocator must store a NEW random tag over the granule (poison) so any
 *     stale pointer, which still carries the old tag, faults on the next
 *     access.
 *   - free() that just records the chunk in a free list and returns leaves
 *     the granule tag untouched. A stale pointer then still matches the
 *     granule tag, dereferences "fine", and the use-after-free is silent --
 *     the very class MTE exists to catch.
 *
 * Correct fix: on free, IRG a fresh tag and STG it over the granule range
 * (Scudo does this automatically when MTE is enabled).
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>

extern uint64_t irg(uint64_t); /* pseudo-op for illustration */

struct chunk { void *p; };

static void bad_free(struct chunk *c, void *p)
{
    /* WRONG: no tag change here. */
    c->p = p;
    free(p); /* granule keeps its old allocation tag; stale pointers work */
}

static void good_free(void *p)
{
    /* poison: store a fresh random tag over the granule(s) */
    uint64_t fresh = irg(0);      /* IRG new tag */
    __asm__ volatile("stg %0, [%1]" : : "r"(fresh), "r"(p));
    free(p); /* now a stale pointer with the old tag faults on access */
}
