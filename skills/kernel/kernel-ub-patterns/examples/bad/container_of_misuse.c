/*
 * BAD: // intentionally incorrect — container_of on a pointer that is
 * NOT the member of the target struct. The offset computation yields a
 * bogus "base" with no compile-time diagnostic. Provenance must be
 * traced (list macros guarantee the relationship); a wrong member name
 * silently changes the offset. (gcc -Warray-bounds may flag the bogus
 * read here; the fixture is compiled with -Wno-array-bounds so the
 * silent-corruption behavior can be demonstrated.)
 *
 * Build: gcc -Wall -Wextra -O2 -Wno-array-bounds container_of_misuse.c -o containerbad
 */
#include <stddef.h>
#include <stdio.h>

#define container_of(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

struct unrelated {
    int dummy;
    long data;
};

struct drv {
    int id;
    int pad[3];          /* pushes 'node' to a nonzero offset */
    long node;
};

int main(void) {
    struct unrelated u = { .dummy = 1, .data = 0xCAFEBABE };
    struct drv d = { .id = 7, .pad = {0,0,0}, .node = 0 };

    /* // intentionally incorrect: u.data is not drv.node */
    struct drv *fake = container_of(&u.data, struct drv, node);

    printf("fake->id = %d (should be unreachable in correct code)\n", fake->id);
    if (fake == &d) {
        printf("BUG hidden: happened to align\n");
    }
    return 0;
}
