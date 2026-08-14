// GOOD: correct C++20 <atomic> usage. Compile:
//   g++ -std=c++20 -Wall -Wextra -Werror -O2 cpp20_atomic_good.cpp -o out && ./out
// Teaching: expected is in-out; success/failure orders are explicit; the
// single-order overload derives a legal failure order (acq_rel -> acquire,
// release -> relaxed); lock-freedom is a compile-time query when possible.
#include <atomic>
#include <cstdio>

// G1: CAS loop with explicit success/failure orders. On failure `expected` is
// overwritten with the observed value — use it, never the stale assumption.
static int publish_once(std::atomic<int>& slot) {
    int expected = 0;
    while (!slot.compare_exchange_weak(expected, 1,
                                       std::memory_order_acq_rel,
                                       std::memory_order_acquire)) {
        if (expected != 0) {
            return expected; // someone else already set it
        }
    }
    return 1;
}

// G2: one-shot transition uses strong CAS (no spurious failure).
static bool claim_once(std::atomic<int>& slot) {
    int expected = 0;
    return slot.compare_exchange_strong(expected, 1);
}

// G3: relaxed is correct for a lossy stats counter.
static void bump(std::atomic<unsigned long>& c) {
    c.fetch_add(1, std::memory_order_relaxed);
}

// G4: release/acquire publish protocol.
static std::atomic<int> g_ready{0};
static int g_data = 0;
static void publish_good() {
    g_data = 42;
    g_ready.store(1, std::memory_order_release);
}
static int consume_good() {
    if (g_ready.load(std::memory_order_acquire) == 0) return -1;
    return g_data;
}

// G5: compile-time lock-freedom query (C++17+).
static constexpr bool int_always_lockfree = std::atomic<int>::is_always_lock_free;

int main() {
    std::atomic<int> slot{0};
    int r = publish_once(slot);
    std::printf("publish_once r=%d slot=%d\n", r, slot.load());
    std::atomic<int> s2{0};
    bool c2 = claim_once(s2);
    std::printf("claim_once=%d slot=%d\n", (int)c2, s2.load());
    std::atomic<unsigned long> hits{0};
    bump(hits);
    bump(hits);
    bump(hits);
    std::printf("hits=%lu\n", hits.load());
    publish_good();
    std::printf("consume=%d\n", consume_good());
    std::printf("is_always_lock_free=%d\n", (int)int_always_lockfree);
    return 0;
}
