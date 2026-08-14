// BAD: C11 atomic mistakes. Compiles (that is the trap), semantics wrong.
//   gcc -std=c11 -Wall -Wextra -Werror -O2 c11_atomic_bad.c -o out && ./out
// Teaching: each function shows one bug class; the prints expose the bug.
#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>

// B1: expected-value misuse. After a failed CAS, *expected holds the observed
// value, not the caller's assumption. Branching on `expected == 0` can never
// fire once the comparison failed.
static int assume_unchanged(_Atomic int *slot) {
    int expected = 0;
    if (atomic_compare_exchange_strong(slot, &expected, 1)) {
        return 0; // won
    }
    return expected == 0 ? 1 : 2; // always 2: expected was overwritten
}

// B2: one-shot gate with the weak form. Weak may fail spuriously even when the
// slot is still 0, silently skipping the claim.
static int one_shot_weak(_Atomic int *slot) {
    int expected = 0;
    return atomic_compare_exchange_weak(slot, &expected, 1);
}

// B3: relaxed flag protocol — the reader may observe ready==true while the
// non-atomic data is still stale (missing synchronizes-with edge).
static _Atomic bool g_ready_bad;
static int g_data_bad;
static void pub_bad(void) {
    g_data_bad = 42;
    atomic_store_explicit(&g_ready_bad, true, memory_order_relaxed);
}
static int consume_bad(void) {
    if (!atomic_load_explicit(&g_ready_bad, memory_order_relaxed)) return -1;
    return g_data_bad;
}

// B4: volatile used for inter-thread sync — still a data race.
static volatile int g_flag_v;
static int g_data_v;
static void pub_v(void) {
    g_data_v = 1;
    g_flag_v = 1;
}
static int consume_v(void) {
    if (!g_flag_v) return -1;
    return g_data_v;
}

// B5: assumes atomic_int is lock-free without checking. On targets where
// ATOMIC_INT_LOCK_FREE is 1 the fetch_add may lower to a libatomic call and
// is not safe in a signal handler.
static _Atomic int g_count_bad;
static void handler_bad(void) {
    atomic_fetch_add_explicit(&g_count_bad, 1, memory_order_relaxed);
}

int main(void) {
    _Atomic int s = 0;
    s = 5;
    int au = assume_unchanged(&s);
    printf("assume_unchanged=%d slot=%d\n", au, atomic_load(&s));
    _Atomic int g2 = 0;
    int ow = one_shot_weak(&g2);
    printf("one_shot_weak=%d slot=%d\n", ow, atomic_load(&g2));
    pub_bad();
    printf("consume_bad=%d\n", consume_bad());
    pub_v();
    printf("consume_v=%d\n", consume_v());
    handler_bad();
    printf("count=%d\n", atomic_load(&g_count_bad));
    return 0;
}
