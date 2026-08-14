// GOOD: correct release/acquire flag protocol.
#include <stdatomic.h>
#include <stdbool.h>

extern _Atomic bool ready;
extern int data;

// G1: release-store AFTER publishing data; acquire-load BEFORE reading it.
void pub_good(void) { data = 42; atomic_store_explicit(&ready, true, memory_order_release); }
int consume_good(void) {
    if (!atomic_load_explicit(&ready, memory_order_acquire)) return -1;
    return data; // SAFE: release store happened-before this acquire load
}

// G2: atomic counter.
extern _Atomic int counter;
void inc_good(void) { atomic_fetch_add_explicit(&counter, 1, memory_order_relaxed); }

// G3: stats counter is a legitimate relaxed use.
extern _Atomic unsigned long hits;
void hit(void) { atomic_fetch_add_explicit(&hits, 1, memory_order_relaxed); }
