// intentionally incorrect — BAD example: missing StoreCap permission on a
// capability store.
//
// Storing a capability (not just data) requires the StoreCap permission on the
// destination. If the object was created without StoreCap, the store faults
// even though the address and bounds are valid. The agent must check the
// permission mask, not just bounds.

#include <stdlib.h>

typedef struct {
    void *payload;
} holder_t;

int main(void)
{
    holder_t *h = (holder_t *)malloc(sizeof(holder_t));

    // BUG: assume h allows capability stores (StoreCap). If the allocator or a
    // narrowed bounds_set dropped store_cap, this store faults. Correct code
    // would verify the permission mask (cheri_perms_get) before the store.
    h->payload = (void *)malloc(64);

    free(h->payload);
    free(h);
    return 0;
}
