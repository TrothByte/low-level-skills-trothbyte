// BAD: unbounded loop. gcc and clang both accept this; the kernel verifier
// rejects it at load because the loop bound is not statically provable.
// Verifier log (target): "BPF program is too large. Processed %d insn" (the
// state-exploration limit catches the unprovable loop).
// Host: gcc -Wall -Wextra -Werror -O2 -c
#include <stddef.h>
#include <stdint.h>

typedef uint32_t u32;
typedef uint64_t u64;

#define SEC(name) __attribute__((section(name), used))

struct xdp_md {
	void *data;
	void *data_end;
	void *data_meta;
	u32 ingress_ifindex;
	u32 rx_queue_index;
	u32 egress_ifindex;
};

SEC("xdp")
int bad_unbounded_loop(void *ctx)
{
	struct xdp_md *xdp = ctx;
	u64 i;
	u64 n = xdp->rx_queue_index;
	u64 sum = 0;

	/* n has no provable upper bound: the verifier cannot prove termination. */
	for (i = 0; i < n; i++)
		sum += i;

	return sum;
}
