// GOOD: correct C11 <stdatomic.h> usage. Compile:
//   gcc -std=c11 -Wall -Wextra -Werror -O2 c11_atomic_good.c -o out && ./out
// Teaching: expected is in-out; weak needs a loop; strong is for one-shot;
// lock-freedom must be checked; release/acquire must be paired.
#include <stdatomic.h>
#include <stdio.h>

// G1: CAS retry loop. On failure *expected is overwritten with the observed
// value; use it (never the stale pre-call assumption) to decide the retry.
static int publish_once(_Atomic int *slot) {
    int expected = 0;
    while (!atomic_compare_exchange_weak(slot, &expected, 1)) {
        if (expected != 0) {
            return expected; // someone else already set it
        }
    }
    return 1;
}

// G2: one-shot transition uses the strong form (no spurious failure).
static int claim_once(_Atomic int *slot) {
    int expected = 0;
    return atomic_compare_exchange_strong(slot, &expected, 1);
}

// G3: relaxed is correct for a lossy stats counter.
static void bump(_Atomic unsigned long *c) {
    atomic_fetch_add_explicit(c, 1, memory_order_relaxed);
}

// G4: release/acquire publish protocol. Release store must come AFTER the data
// write; the acquire load must come BEFORE reading it back.
static _Atomic int g_ready;
static int g_data;
static void publish_good(void) {
    g_data = 42;
    atomic_store_explicit(&g_ready, 1, memory_order_release);
}
static int consume_good(void) {
    if (!atomic_load_explicit(&g_ready, memory_order_acquire)) return -1;
    return g_data;
}

// G5: check lock-freedom at compile time before relying on it in a handler.
#if ATOMIC_INT_LOCK_FREE == 2
static int lockfree_ok = 1;
#else
static int lockfree_ok = 0;
#endif

int main(void) {
    _Atomic int slot = 0;
    int r = publish_once(&slot);
    printf("publish_once r=%d slot=%d\n", r, atomic_load(&slot));
    _Atomic int s2 = 0;
    int c = claim_once(&s2);
    printf("claim_once=%d slot=%d\n", c, atomic_load(&s2));
    _Atomic unsigned long hits = 0;
    bump(&hits);
    bump(&hits);
    bump(&hits);
    printf("hits=%lu\n", atomic_load(&hits));
    publish_good();
    printf("consume=%d\n", consume_good());
    printf("lockfree_ok=%d\n", lockfree_ok);
    return 0;
}
