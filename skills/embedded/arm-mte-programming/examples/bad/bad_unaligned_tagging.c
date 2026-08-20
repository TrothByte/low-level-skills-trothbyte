/*
 * bad_unaligned_tagging.c -- WRONG: tagging an allocation that is not
 * 16-byte aligned.
 *
 * TARGET-ONLY, annotated for review. NOT compiled here (host is x86 MinGW;
 * MTE needs ARMv8.5+/Armv9).
 *
 * What is wrong:
 *   - malloc() guarantees only sizeof(max_align_t) alignment (often 16 on
 *     aarch64, but NOT guaranteed, and 8-byte alignments are common).
 *   - The allocation tag lives on the 16-byte granule that CONTAINS the
 *     allocation. If the object starts at offset 8 within the granule, the
 *     granule's tag also covers the 8 bytes BEFORE the object.
 *   - An overwrite that stays inside the same granule but before the object
 *     boundary, or an intra-granule neighbor, silently shares the tag and is
 *     NOT caught -- the allocator must align allocations to 16 so that one
 *     object owns one granule.
 *
 * Correct fix: posix_memalign(&p, 16, size) (or an aligned allocator such as
 * Scudo with MTE support), then IRG + STG the whole range.
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdlib.h>

extern void irg(uint64_t *out, void *p); /* pseudo-op for illustration */

void bad_alloc(void)
{
    void *p = malloc(32); /* alignment not guaranteed to be 16 */
    uint64_t tag;
    irg(&tag, p);
    /* Tag STG here would tag the granule that contains p, including up to
     * 15 bytes before p if p is not 16-aligned. p now shares its tag with
     * unrelated data in the same granule. */
    (void)p;
    (void)tag;
}
