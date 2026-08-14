// GOOD patterns: the same situations written the kernel way.
// Compile: gcc -Wall -Wextra -Werror -O2 -c good_snippets.c
#include "../linux_stubs.h"

struct item {
    int value;
    struct item *next;
};

static struct item *g_item;   // shared: published via RCU
static int g_once_guard;      // one-shot latch, no payload
static int g_ready;           // shared flag (WRITE_ONCE/READ_ONCE + smp_mb)
static int g_data[4];         // payload published before g_ready

// G1: READ_ONCE/WRITE_ONCE. A one-shot guard with no payload needs only
// single-access semantics: WRITE_ONCE/READ_ONCE prevent tearing, merging, and
// register caching. Do NOT mistake them for barriers -- ordering for a payload
// comes from smp_mb()/RCU (see G2, G3).
int once_writer(void)
{
    WRITE_ONCE(g_once_guard, 1);
    return 0;
}

int once_reader(void)
{
    return READ_ONCE(g_once_guard) != 0;
}

// G2: RCU publish-subscribe. rcu_assign_pointer() is a release store, so the
// payload writes before it are visible to any reader that sees the new
// pointer. The reader calls rcu_dereference() inside rcu_read_lock(), and the
// object cannot be freed (by a later synchronize_rcu()/call_rcu()) while it is
// in use.
int publish_subscribe(struct item *new_item)
{
    new_item->value = 42;                // plain store: ordered by the release
    rcu_assign_pointer(g_item, new_item); // release: publishes payload first
    return 0;
}

int subscribe_reader(void)
{
    int v = -1;
    rcu_read_lock();
    struct item *p = rcu_dereference(g_item);  // acquire/dependency ordering
    if (p) {
        v = p->value;                    // safe: object cannot be freed yet
    }
    rcu_read_unlock();
    return v;
}

// G3: explicit smp_mb() ordering for a flag+payload protocol. The reader's
// smp_rmb() pairs with the writer's smp_wmb(): the reader sees the payload
// only after the flag flips. On x86 the hardware part is free (TSO); on
// ARM/RISC-V the stubs would be real hardware fences.
int publish_mb(int v)
{
    g_data[0] = v;
    smp_wmb();                           // payload stores before flag store
    WRITE_ONCE(g_ready, 1);
    return 0;
}

int read_mb(void)
{
    if (READ_ONCE(g_ready)) {
        smp_rmb();                       // flag load before payload loads
        return READ_ONCE(g_data[0]);
    }
    return -1;
}
