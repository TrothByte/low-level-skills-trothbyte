// GOOD: bounds established before the packet access -- the verifier-visible
// pattern (see the ebpf-verifier-reasoning skill). After this change the
// load loop is: extract (message names R1, r=0) -> minimize -> bisect (r=0
// created by the ctx->data load) -> insert data_end check -> re-verify PASS.
// Host sanity: gcc -Wall -Wextra -Werror -O2 -c (exit 0, verified).
// Target (Linux): clang -O2 -g -target bpf -c && bpftool prog load
#include <stdint.h>

typedef uint32_t u32;
typedef uint64_t u64;

#define SEC(name) __attribute__((section(name), used))

struct xdp_md {
	u64 data;
	u64 data_end;
};

SEC("xdp")
int good_bounds_checked(struct xdp_md *ctx)
{
	void *data = (void *)ctx->data;
	void *data_end = (void *)ctx->data_end;
	u64 proto;

	if ((char *)data + 8 > (char *)data_end)
		return 0;
	proto = *(u64 *)data;
	(void)proto;
	return 0;
}
