// BAD: C++20 atomic mistakes. Compiles (that is the trap), semantics wrong;
// one variant is undefined behavior that g++ accepts silently.
//   g++ -std=c++20 -Wall -Wextra -Werror -O2 cpp20_atomic_bad.cpp -o out && ./out
// Teaching: each function shows one bug class; the prints expose the bugs.
#include <atomic>
#include <cstdio>

// B1: expected-value misuse. After a failed CAS, `expected` holds the observed
// value; branching on the pre-CAS assumption (`expected == 0`) can never fire.
static int assume_unchanged(std::atomic<int>& slot) {
    int expected = 0;
    if (slot.compare_exchange_strong(expected, 1)) {
        return 0; // won
    }
    return expected == 0 ? 1 : 2; // always 2: expected was overwritten
}

// B2: one-shot gate with weak CAS. Weak may fail spuriously even when the slot
// is still 0, silently skipping the claim.
static bool one_shot_weak(std::atomic<int>& slot) {
    int expected = 0;
    return slot.compare_exchange_weak(expected, 1);
}

// B3: invalid failure memory order. Precondition: failure must be relaxed,
// acquire, or seq_cst — release is UB (a failure path is a load; it can never
// be release-ordered). g++ accepts this without a diagnostic.
static bool bad_failure_order(std::atomic<int>& slot, int desired) {
    int expected = 0;
    return slot.compare_exchange_strong(expected, desired,
                                        std::memory_order_acq_rel,
                                        std::memory_order_release);
}

// B4: relaxed flag protocol — missing synchronizes-with edge.
static std::atomic<int> g_ready_bad{0};
static int g_data_bad = 0;
static void pub_bad() {
    g_data_bad = 42;
    g_ready_bad.store(1, std::memory_order_relaxed);
}
static int consume_bad() {
    if (g_ready_bad.load(std::memory_order_relaxed) == 0) return -1;
    return g_data_bad;
}

// B5: volatile flag used for inter-thread sync — still a data race.
static volatile int g_flag_v;
static int g_data_v;
static void pub_v() {
    g_data_v = 1;
    g_flag_v = 1;
}
static int consume_v() {
    if (!g_flag_v) return -1;
    return g_data_v;
}

int main() {
    std::atomic<int> s{5};
    int au = assume_unchanged(s);
    std::printf("assume_unchanged=%d slot=%d\n", au, s.load());
    std::atomic<int> g2{0};
    bool w = one_shot_weak(g2);
    std::printf("one_shot_weak=%d slot=%d\n", (int)w, g2.load());
    std::atomic<int> g3{0};
    bool b = bad_failure_order(g3, 1);
    std::printf("bad_failure_order=%d slot=%d\n", (int)b, g3.load());
    pub_bad();
    std::printf("consume_bad=%d\n", consume_bad());
    pub_v();
    std::printf("consume_v=%d\n", consume_v());
    return 0;
}
