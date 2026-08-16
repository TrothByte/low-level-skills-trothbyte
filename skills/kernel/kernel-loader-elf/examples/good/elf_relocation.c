/*
 * GOOD: apply relative relocations to a position-independent image.
 * ET_DYN images need a load bias added to every PT_LOAD vaddr and to
 * each R_*_RELATIVE fixup (B + A: load base + addend, base symbol = 0).
 * This fixture models the relocation application and verifies the
 * resulting pointers point into the relocated image.
 *
 * Build: gcc -Wall -Wextra -Werror -O2 elf_relocation.c -o elfrel
 */
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

/* .rela.dyn entry: r_offset, r_info, r_addend. Type = R_X86_64_RELATIVE. */
typedef struct {
    uint64_t r_offset;
    uint64_t r_info;
    int64_t  r_addend;
} rela_t;

#define R_X86_64_RELATIVE 8

int main(void) {
    /* Link-time (p_vaddr) image at 0x200000, loaded at 0x8000000. */
    uint64_t link_base = 0x200000;
    uint64_t bias = 0x7E00000;             /* chosen load delta */
    uint64_t load_base = link_base + bias; /* 0x8000000 */

    /* Runtime image memory: this array simulates memory at load_base.
       A slot at link-time offset r_offset lives at array index r_offset/8
       (the sim array starts at 0 == load_base, so index = offset - bias
       collapses to the same value for these small offsets). */
    uint64_t mem[8] = {0};
    mem[0] = link_base + 0x100;   /* GOT slot 0: points at link-time addr */
    mem[1] = link_base + 0x200;

    /* Two relative relocations. */
    rela_t relas[] = {
        {0x00, (uint64_t)R_X86_64_RELATIVE << 32, 0x100}, /* fix mem[0] */
        {0x08, (uint64_t)R_X86_64_RELATIVE << 32, 0x200}, /* fix mem[1] */
    };

    /* GOOD: apply each relative relocation.
       R_X86_64_RELATIVE = B + A: load base + addend (base symbol S = 0).
       The slot address (r_offset) is link-time; runtime address is
       r_offset + bias, which maps to array index r_offset/8 in this sim. */
    for (int i = 0; i < 2; i++) {
        assert((relas[i].r_info >> 32) == R_X86_64_RELATIVE);
        uint64_t *slot = (uint64_t *)((uint8_t *)mem + relas[i].r_offset);
        *slot = load_base + relas[i].r_addend;   /* B + A */
    }

    assert(mem[0] == load_base + 0x100);
    assert(mem[1] == load_base + 0x200);
    printf("PASS: relative relocations relocated to load base %#llx\n",
           (unsigned long long)load_base);
    return 0;
}
