// BAD: packet data dereferenced without establishing bounds against
// data_end. Compiles with gcc (C sanity), but the kernel verifier rejects it
// at load with "invalid access to packet, off=0 size=8, R2(id=0,off=0,r=0)".
// The register state r=0 in the message is the diagnostic: no bounds were
// established. See examples/bad/bad_give_up_on_opaque_log.py for the
// give-up failure mode this triggers.
// // intentionally incorrect
//
// Host sanity: gcc -Wall -Wextra -Werror -O2 -c
// Target (Linux): clang -O2 -g -target bpf -c && bpftool prog load -d
#include <stdint.h>

typedef uint32_t u32;
typedef uint64_t u64;

#define SEC(name) __attribute__((section(name), used))

struct xdp_md {
	u64 data;
	u64 data_end;
};

SEC("xdp")
int bad_casts_away_bounds(struct xdp_md *ctx)
{
	void *data = (void *)ctx->data;
	u64 proto = *(u64 *)data;   /* no data_end check: verifier rejects */
	(void)proto;
	return 0;
}
