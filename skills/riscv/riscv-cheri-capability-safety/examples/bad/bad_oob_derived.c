// intentionally incorrect — BAD example: pointer arithmetic escapes the
// capability bounds, clearing the tag.
//
// `p + (n + 1)` derives an address past the object. CHERI clears the tag the
// moment the arithmetic leaves the capability's bounds — even if the final
// numeric address would be inside another valid allocation. The subsequent
// dereference faults with a CHERI tag/bounds fault (SIGPROT in CheriBSD).
//
// Target toolchain: QEMU-CHERI + cheribuild (absent here; documented command:
// cheribuild run-sdk --cheribsd -- purecap-cc examples/bad/bad_oob_derived.c).

#include <stdint.h>
#include <stdlib.h>

int *bad_escape(int *p, size_t n)
{
    // BUG: stepping past the object's end clears the tag.
    return p + (n + 1);
}

int main(void)
{
    int *buf = (int *)malloc(16 * sizeof(int));
    int *tail = bad_escape(buf, 16);   // p + 17 -> out of bounds
    *tail = 42;                        // CHERI tag fault at runtime
    free(buf);
    return 0;
}
