// BAD patterns: each function contains a classic kernel memory-ordering or
// RCU mistake. They compile on the host (syntactically valid) but are broken
// on a real kernel. Compile: gcc -Wall -Wextra -Werror -O2 -c bad_snippets.c
#include "../linux_stubs.h"

struct item {
    int value;
    struct item *next;
};

static struct item *g_item;   // shared: written by writers, read by readers
static int g_ready;           // shared flag
static int g_data[4];         // payload published before g_ready

// B1: missing WRITE_ONCE + missing barrier in a flag+payload protocol.
// Plain `g_data[i] = v` and `g_ready = 1` are a data race (UB) and give no
// ordering: the reader may see g_ready==1 before the data stores land, and
// the compiler may cache or tear the plain accesses.
int publish_flag_bad(int v)
{
    g_data[0] = v;            // plain store: can be reordered or fused
    g_ready = 1;              // plain store: no release, not even volatile
    return 0;
}

int read_flag_bad(void)
{
    if (g_ready) {            // plain load: no acquire, not even volatile
        return g_data[0];     // may observe a stale or partial payload
    }
    return -1;
}

// B2: RCU reader taking a raw copy of the pointer instead of rcu_dereference().
// The write side publishes with rcu_assign_pointer(); the reader must load the
// pointer atomically via rcu_dereference() inside rcu_read_lock(), not via a
// plain load. This plain load is a data race and gets no ordering.
int read_rcu_raw(void)
{
    struct item *p = g_item;  // BUG: plain load, no rcu_dereference
    if (p) {
        return p->value;      // also no rcu_read_lock() around the deref
    }
    return -1;
}

// B3: publishing a pointer without a barrier: the payload writes must be
// visible before the pointer is published (rcu_assign_pointer / smp_wmb).
void publish_without_barrier(struct item *new_item)
{
    new_item->value = 7;      // plain store: can be reordered after the publish
    g_item = new_item;        // plain store: no release ordering
}

// B4: kmalloc(GFP_KERNEL) inside a spinlock. GFP_KERNEL may sleep (page
// reclaim); sleeping while holding a spinlock is a sleep-in-atomic-context
// bug (deadlock on SMP, or a CONFIG_DEBUG_ATOMIC_SLEEP splat).
void alloc_inside_lock(spinlock_t *lock)
{
    spin_lock(lock);
    void *buf = kmalloc(4096, GFP_KERNEL);  // BUG: may sleep in atomic context
    if (buf) {
        kfree(buf);
    }
    spin_unlock(lock);
}
