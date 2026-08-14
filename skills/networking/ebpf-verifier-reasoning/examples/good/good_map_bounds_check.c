// GOOD: correct bpf_map_lookup_elem() + bounds checks. The returned map value
// is null-checked, and the index into the fixed-size array field is
// range-checked before use, so the verifier proves both the NULL state and the
// offset bounds (idx in [0, 8) means off+size <= value_size).
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
int good_map_bounds_check(void *ctx)
{
	struct xdp_md *xdp = ctx;
	struct bucket_t *b;
	u32 key = 0;
	u32 idx = xdp->rx_queue_index;

	b = bpf_map_lookup_elem(&buckets_map, &key);
	if (!b)
		return 0;
	if (idx >= 8)
		return 0;
	b->counters[idx] += 1;

	return 0;
}
