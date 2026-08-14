// BAD: direct packet access without a data/data_end bounds check. The verifier
// can only assign a safe range to the packet pointer after a compare it can
// see; here no such check exists, so the load is rejected.
// Verifier log (target): "invalid access to packet, off=0 size=1,
// R3(id=0,off=0,r=0)" (r=0 means zero proven bytes).
// Host: gcc -Wall -Wextra -Werror -O2 -c
#include <stddef.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
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

struct iphdr {
	u8 version_ihl;
	u8 tos;
	u16 tot_len;
	u16 id;
	u16 frag_off;
	u8 ttl;
	u8 protocol;
	u16 check;
	u32 saddr;
	u32 daddr;
};

SEC("xdp")
int bad_packet_without_bounds(void *ctx)
{
	struct xdp_md *xdp = ctx;
	void *data = xdp->data;
	struct iphdr *ip = (struct iphdr *)data;

	return ip->protocol;
}
