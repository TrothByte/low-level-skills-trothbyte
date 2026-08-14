// BAD: unbounded scalar index into a fixed-size array inside the map value.
// The map value is 8 * sizeof(u32) + sizeof(u32) bytes; idx is packet-derived
// with no provable range, so the verifier cannot prove the access stays inside
// value_size and rejects it.
// Verifier log (target): "R0 unbounded memory access, make sure to bounds
// check any such access" (variable offset into map value).
// Host: gcc -Wall -Wextra -Werror -O2 -c
#include <stddef.h>
#include <stdint.h>

typedef uint32_t u32;
typedef uint64_t u64;

#define SEC(name) __attribute__((section(name), used))
#define __uint(name, val) int (*name)[val]
#define __type(name, val) val *name

#define BPF_MAP_TYPE_ARRAY 2

struct bucket_t {
	u32 counters[8];
	u32 total;
};

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 16);
	__type(key, u32);
	__type(value, struct bucket_t);
} buckets_map SEC(".maps");

static void *(* const bpf_map_lookup_elem)(void *map, const void *key) = (void *) 1;

struct xdp_md {
	void *data;
	void *data_end;
	void *data_meta;
	u32 ingress_ifindex;
	u32 rx_queue_index;
	u32 egress_ifindex;
};

SEC("xdp")
int bad_map_index_oob(void *ctx)
{
	struct xdp_md *xdp = ctx;
	struct bucket_t *b;
	u32 key = 0;
	u32 idx = xdp->rx_queue_index;

	b = bpf_map_lookup_elem(&buckets_map, &key);
	if (!b)
		return 0;

	/* idx is unbounded: offset into the map value is not provably in range. */
	b->counters[idx] += 1;

	return 0;
}
