// BAD: map value dereferenced before a NULL check on the bpf_map_lookup_elem()
// result. The helper returns PTR_TO_MAP_VALUE_OR_NULL; the verifier rejects the
// unchecked dereference even though ARRAY elements are pre-allocated.
// Verifier log (target): "R0 invalid mem access 'map_value_or_null'".
// Host: gcc -Wall -Wextra -Werror -O2 -c
#include <stddef.h>
#include <stdint.h>

typedef uint32_t u32;
typedef uint64_t u64;

#define SEC(name) __attribute__((section(name), used))
#define __uint(name, val) int (*name)[val]
#define __type(name, val) val *name

#define BPF_MAP_TYPE_ARRAY 2

struct counter_t {
	u64 hits;
	u64 bytes;
};

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, u32);
	__type(value, struct counter_t);
} counters_map SEC(".maps");

static void *(* const bpf_map_lookup_elem)(void *map, const void *key) = (void *) 1;

SEC("xdp")
int bad_missing_null_check(void *ctx)
{
	struct counter_t *v;
	u32 key = 0;

	(void)ctx;

	v = bpf_map_lookup_elem(&counters_map, &key);
	v->hits += 1;

	return 0;
}
