// GOOD: loop bound is made provably finite by clamping the counter before the
// loop. After "if (n > 64) n = 64;" the verifier knows n in [0, 64], so
// termination is proven and the loop is accepted (bounded loops, kernel 5.3+).
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
int good_bounded_loop(void *ctx)
{
	struct xdp_md *xdp = ctx;
	u64 i;
	u64 n = xdp->rx_queue_index;
	u64 sum = 0;

	if (n > 64)
		n = 64;

	for (i = 0; i < n; i++)
		sum += i;

	return sum;
}
