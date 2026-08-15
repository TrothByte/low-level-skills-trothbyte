// intentionally incorrect — BAD example: byte-wise copy of capability storage
// clears the tag.
//
// In purecap, pointers are capabilities; the tag bit lives beside the data, not
// inside it. memcpy copies only the payload, so the destination pointer's tag is
// cleared. Dereferencing the copy faults. Capability arrays must be copied
// element-wise.
//
// Target toolchain: QEMU-CHERI + cheribuild (absent here; documented).

#include <stdlib.h>
#include <string.h>

typedef struct {
    void *ptr;
} wrapper_t;

int main(void)
{
    wrapper_t src = {malloc(64)};
    wrapper_t dst;

    // BUG: byte-wise copy drops the tag from src.ptr.
    memcpy(&dst, &src, sizeof(dst));

    /* dereferencing dst.ptr faults: tag cleared by memcpy */
    (void)*(char *)dst.ptr;
    free(src.ptr);
    return 0;
}
