// GOOD: bounded sub-pointer within bounds — the intended CHERI pattern.
//
// cheri_bounds_set(p, size) narrows the capability to exactly the sub-object;
// cheri_offset_set(p, off) moves within it. As long as the accessed range stays
// inside the bounds, the derived pointer is safe. This is correct code and must
// NOT be flagged by reviewers (FP rule). An over-wide bounds_set (length > the
// object) WOULD be a real bug.
//
// Target toolchain: QEMU-CHERI + cheribuild (absent here; documented).

#include <stdint.h>
#include <stdlib.h>

typedef struct {
    int count;
    int values[8];
} record_t;

// Return a capability bounded to the 'values' array only.
int *record_values(record_t *r)
{
    // Bounds cover exactly the values array: 8 * sizeof(int) bytes.
    int *base = (int *)cheri_bounds_set(r->values, 8 * sizeof(int));
    return base;
}

int main(void)
{
    record_t *r = (record_t *)malloc(sizeof(record_t));
    int *v = record_values(r);            // valid bounded sub-pointer
    for (int i = 0; i < 8; i++)
        v[i] = i;                          // in bounds -> no fault

    // Hash with the numeric address, not an integer round-trip.
    uintptr_t addr = cheri_address_get(v);
    (void)addr;

    free(r);
    return 0;
}
