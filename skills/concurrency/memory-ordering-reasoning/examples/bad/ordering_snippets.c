// BAD: memory-ordering mistakes (conceptual; use TSan/Miri to confirm).
#include <stdatomic.h>
#include <stdbool.h>

// B1: relaxed flag protocol — reader may see data stale.
extern _Atomic bool ready;
extern int data;

void pub_bad(void) { data = 42; atomic_store_explicit(&ready, true, memory_order_relaxed); }
int consume_bad(void) {
    if (!atomic_load_explicit(&ready, memory_order_relaxed)) return -1;
    return data; // RACE: no happens-before edge
}

// B2: non-atomic shared counter — data race (C has no guard; TSan catches).
int counter = 0;
void inc_bad(void) { counter++; } // must be atomic_fetch_add

// B3: volatile used for inter-thread sync — still a data race.
volatile int flag_v;
void pub_v(void) { data = 42; flag_v = 1; }
int consume_v(void) { if (!flag_v) return -1; return data; }
