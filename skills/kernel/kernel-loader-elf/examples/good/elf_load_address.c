/*
 * GOOD: boot-time ELF load-address math (host-runnable core).
 * Computes the runtime mapping for each PT_LOAD: page-aligned base,
 * file-offset congruence (p_vaddr % p_align == p_offset % p_align),
 * and BSS zero-fill of [p_vaddr+p_filesz, p_vaddr+p_memsz).
 *
 * Build: gcc -Wall -Wextra -Werror -O2 elf_load_address.c -o elfaddr
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

typedef struct {
    uint64_t p_vaddr;
    uint64_t p_offset;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} pt_load_t;

typedef struct {
    uint64_t runtime_base;   /* bias for ET_DYN, 0 for ET_EXEC */
    pt_load_t segs[4];
    int nsegs;
} image_t;

static uint64_t round_down(uint64_t x, uint64_t a) {
    return x & ~(a - 1);
}

int main(void) {
    /* Synthetic ET_EXEC kernel image, classic layout. */
    image_t img = {
        .runtime_base = 0,
        .nsegs = 3,
        .segs = {
            /*   vaddr      offset   filesz   memsz    align */
            {0x00100000, 0x00001000, 0x00002000, 0x00002000, 0x1000}, /* text R-X */
            {0x00103000, 0x00003000, 0x00001000, 0x00004000, 0x1000}, /* data RW, 0x3000 BSS */
            {0x00107000, 0x00004000, 0x00002000, 0x00002000, 0x1000}, /* rodata R-- */
        }
    };

    for (int i = 0; i < img.nsegs; i++) {
        pt_load_t *s = &img.segs[i];
        uint64_t base = round_down(s->p_vaddr + img.runtime_base, s->p_align);
        /* Congruence: the offset within the base page must match p_offset's. */
        assert((s->p_vaddr + img.runtime_base) % s->p_align ==
               s->p_offset % s->p_align);
        /* BSS tail must be zeroed. */
        if (s->p_memsz > s->p_filesz) {
            uint8_t scratch[0x4000];
            memset(scratch, 0xAA, sizeof scratch);  /* dirty RAM */
            /* loader would zero [.., ..+memsz-filesz) here */
            memset(scratch + s->p_filesz, 0, s->p_memsz - s->p_filesz);
            for (uint64_t j = s->p_filesz; j < s->p_memsz; j++)
                assert(scratch[j] == 0);
        }
        printf("seg%d: base=%#llx filesz=%#llx bss=%#llx\n",
               i, (unsigned long long)base,
               (unsigned long long)s->p_filesz,
               (unsigned long long)(s->p_memsz - s->p_filesz));
    }
    printf("PASS: load addresses computed with alignment + BSS zero-fill\n");
    return 0;
}
