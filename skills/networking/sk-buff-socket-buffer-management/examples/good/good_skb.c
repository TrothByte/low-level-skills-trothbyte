/* GOOD: correct sk_buff buffer management.
 * alloc -> reserve headroom -> put payload -> push a header -> pull it when
 * parsing -> clone (struct + cb copied, data shared) -> modify only the
 * cloned control block -> free each holder exactly once. Invariants on
 * headroom / data / tailroom / refcounts are asserted at every step.
 */
#include "../stubs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
	struct sk_buff *skb, *clone, *nskb;
	unsigned char *p;

	/* 1. alloc a 128-byte skb: 0 headroom, 0 data, 128 tailroom */
	skb = skb_alloc_emu(128);
	assert(skb != NULL);
	assert(skb_headroom_emu(skb) == 0);
	assert(skb_len_emu(skb) == 0);
	assert(skb_tailroom_emu(skb) == 128);
	assert(skb->head <= skb->data && skb->data <= skb->tail &&
	       skb->tail <= skb->end);

	/* 2. reserve 32 bytes of headroom for protocol headers */
	skb_reserve_emu(skb, 32);
	assert(skb_headroom_emu(skb) == 32);
	assert(skb_len_emu(skb) == 0);
	assert(skb_tailroom_emu(skb) == 96);

	/* 3. put a 64-byte payload into the data area */
	p = skb_put_emu(skb, 64);
	assert(p != NULL);
	assert(p == skb->head + 32);
	memset(p, 0x5A, 64);
	assert(skb_len_emu(skb) == 64);
	assert(skb_tailroom_emu(skb) == 32);
	assert(skb_headroom_emu(skb) == 32);

	/* 4. push a 4-byte header into the reserved headroom */
	p = skb_push_emu(skb, 4);
	assert(p != NULL);
	assert(p == skb->head + 28);
	memcpy(p, "HDR1", 4);
	assert(skb_headroom_emu(skb) == 28);
	assert(skb_len_emu(skb) == 68);
	assert(skb_tailroom_emu(skb) == 32);

	/* 5. pull the header back when parsing: data advances to the payload */
	p = skb_pull_emu(skb, 4);
	assert(p != NULL);
	assert(p == skb->head + 32);
	assert(skb_headroom_emu(skb) == 32);
	assert(skb_len_emu(skb) == 64);
	assert(skb_tailroom_emu(skb) == 32);

	/* 6. clone: struct + control block copied, data area shared */
	memcpy(skb->cb, "CB00", 4);
	clone = skb_clone_emu(skb);
	assert(clone != NULL);
	assert(clone->users == 1);
	assert(skb->users == 1);
	assert(clone->shared == 1 && skb->shared == 1);
	assert(clone->area->dataref == 2);	/* data refcount == 2 */
	assert(clone->data == skb->data);	/* same data area, not a copy */
	assert(clone->head == skb->head);
	assert(memcmp(clone->cb, "CB00", 4) == 0);	/* cb copied */

	/* 7. modify only the cloned control block; the shared payload and the
	 *    original cb must be untouched */
	memcpy(clone->cb, "CB11", 4);
	assert(memcmp(skb->cb, "CB00", 4) == 0);
	assert(skb->head[32] == 0x5A);		/* payload byte via original */
	assert(clone->head[32] == 0x5A);	/* same byte via the clone */

	/* 8. share-check: data is shared (dataref == 2), so the struct is
	 *    re-cloned and this holder's reference is dropped */
	nskb = skb_share_check_emu(skb);
	assert(nskb != NULL);
	assert(nskb != skb);
	assert(nskb->users == 1);
	assert(skb->freed == 1);		/* original struct consumed */
	assert(nskb->area->dataref == 2);	/* clone + nskb still hold data */

	/* 9. free each holder exactly once; the data area dies with its last
	 *    holder */
	skb_free_emu(clone);
	assert(clone->freed == 1);
	assert(nskb->area->dataref == 1);
	skb_free_emu(nskb);
	assert(nskb->freed == 1);
	assert(nskb->area == NULL);		/* last holder freed the data */

	/* 10. skb_copy: full data copy — private, safe to modify */
	skb = skb_alloc_emu(64);
	assert(skb != NULL);
	skb_reserve_emu(skb, 8);
	p = skb_put_emu(skb, 16);
	assert(p != NULL);
	memset(p, 0x33, 16);
	nskb = skb_copy_emu(skb);
	assert(nskb != NULL);
	assert(nskb->data != skb->data);	/* private data */
	assert(nskb->area->dataref == 1);
	memset(nskb->data, 0x44, 16);		/* safe: private copy */
	assert(skb->head[8] == 0x33);		/* original untouched */
	skb_free_emu(nskb);
	skb_free_emu(skb);

	assert(skb_emu_bugs == 0);		/* no BUG diagnostics fired */
	printf("ALL CHECKS PASSED\n");
	return 0;
}
