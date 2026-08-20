/* BAD: typical sk_buff misuse, detected at runtime by the simulator.
 * 1) skb_put past the tailroom (no reserve / wrong length) — the kernel
 *    never expands the buffer, so the write would corrupt adjacent memory.
 * 2) writing into skb->data of a shared clone as if it were private —
 *    corrupts the original's payload.
 * 3) double free of a shared skb — wrong ownership accounting: the freed
 *    original is freed again.
 */
#include "../stubs.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
	struct sk_buff *skb, *clone;
	unsigned char *p;
	int before;

	/* BUG 1: skb_put beyond the tailroom */
	skb = skb_alloc_emu(32);
	if (!skb)
		return 1;
	p = skb_put_emu(skb, 64);	/* 64 > 32 tailroom */
	if (p == NULL)
		printf("BUG reproduced: skb_put exceeded tailroom\n");
	skb_free_emu(skb);

	/* BUG 2: shared clone modified as if it owned private data */
	skb = skb_alloc_emu(64);
	if (!skb)
		return 1;
	skb_reserve_emu(skb, 4);
	p = skb_put_emu(skb, 16);
	if (!p)
		return 1;
	memset(p, 'P', 16);
	clone = skb_clone_emu(skb);
	if (!clone)
		return 1;
	/* BAD: clone->data aliases skb->data; this corrupts the original */
	memcpy(clone->data, "CORRUPTED", 8);
	if (memcmp(skb->data, "PPPPPPPP", 8) != 0)
		printf("BUG reproduced: modified shared skb data\n");
	skb_free_emu(clone);
	skb_free_emu(skb);

	/* BUG 3: double free of a shared skb */
	skb = skb_alloc_emu(32);
	if (!skb)
		return 1;
	p = skb_put_emu(skb, 8);
	if (!p)
		return 1;
	clone = skb_clone_emu(skb);
	if (!clone)
		return 1;
	skb_free_emu(skb);		/* original holder gone */
	skb_free_emu(clone);		/* clone holder gone, data freed */
	before = skb_emu_bugs;
	skb_free_emu(skb);		/* BAD: the freed original again */
	if (skb_emu_bugs != before)
		printf("BUG reproduced: double free of a shared skb\n");

	return 0;
}
