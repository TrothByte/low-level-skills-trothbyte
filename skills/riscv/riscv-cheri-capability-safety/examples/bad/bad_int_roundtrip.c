// intentionally incorrect — BAD example: integer round-trip drops the tag.
//
// Casting a capability to uintptr_t and back produces an address-only value
// with the tag cleared. The dereference faults. Use cheri_address_get() for the
// numeric address; never store/restore a capability through an integer.

#include <stdint.h>
#include <stdlib.h>

int main(void)
{
    int *buf = (int *)malloc(16 * sizeof(int));

    // BUG: round-trip through an integer.
    uintptr_t addr = (uintptr_t)buf;
    int *back = (int *)addr;      // tag cleared

    *back = 1;                    // CHERI tag fault at runtime
    free(buf);
    return 0;
}
