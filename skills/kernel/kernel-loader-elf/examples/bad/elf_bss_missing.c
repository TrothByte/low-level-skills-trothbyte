/*
 * BAD: // intentionally incorrect — maps the file bytes but never
 * zero-fills the BSS tail ([p_vaddr+p_filesz, p_vaddr+p_memsz)).
 * The loader then carries stale RAM into kernel globals.
 *
 * Build: gcc -Wall -Wextra -Werror -O2 elf_bss_missing.c -o elfbss
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint64_t p_vaddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
} pt_load_t;

/* Simulated stale RAM that a missing zero-fill would leak. */
static uint8_t ram[0x10000];
static int stale_writes;

static void load_segment(pt_load_t *s, const uint8_t *file_bytes) {
    memcpy(ram + s->p_vaddr, file_bytes, s->p_filesz);
    /* BSS zero-fill omitted: // intentionally incorrect */
}

int main(void) {
    pt_load_t seg = { .p_vaddr = 0x1000, .p_filesz = 0x1000, .p_memsz = 0x8000 };
    static uint8_t file_bytes[0x1000];
    memset(file_bytes, 0x5A, sizeof file_bytes);

    /* Dirty the BSS region before "loading". */
    for (int i = 0; i < 0x10000; i++) ram[i] = 0xCD;
    stale_writes = 0;

    load_segment(&seg, file_bytes);

    /* Check: bytes beyond p_filesz must be zero in a correct loader. */
    int garbage = 0;
    for (uint64_t i = seg.p_vaddr + seg.p_filesz; i < seg.p_vaddr + seg.p_memsz; i++) {
        if (ram[i] != 0) { garbage++; break; }
    }
    if (garbage) {
        printf("BUG: BSS tail contains stale RAM (0x%02x at vaddr+%#llx)\n",
               ram[seg.p_vaddr + seg.p_filesz],
               (unsigned long long)(seg.p_vaddr + seg.p_filesz));
        return 1;
    }
    printf("PASS: BSS zeroed\n");
    return 0;
}
