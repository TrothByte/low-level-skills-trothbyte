---
name: concurrency-deadlock-and-lock-ordering
description: Use when writing or reviewing code that acquires two or more locks — detecting ABBA deadlock, enforcing consistent lock ordering (CON35-C), choosing std::lock/std::scoped_lock, avoiding recursive/try_lock hazards, or interpreting a TSan/helgrind lock-order report.
---

# Deadlock & Lock Ordering

## When to use

- Writing multi-threaded code that locks more than one mutex at a time.
- Reviewing lock acquisition order across functions/threads for a potential cycle.
- Deciding between `std::lock` / `std::scoped_lock` / per-mutex `lock()`.
- Fixing a hang that only happens under load (classic deadlock signature).
- Reading a TSan "lock-order-inversion" or helgrind "lock order violated" report.

## When not to use

- Lock-free or atomic-only synchronization — use `memory-ordering-reasoning`.
- Data races on non-atomic shared data — that is `c-undefined-behavior` territory (race = UB);
  this skill is about liveness (deadlock), not correctness of individual accesses.
- Condition-variable predicate/lost-wakeup bugs — use `concurrency-condvar-and-spurious-wakeup`.
- C `mtx_lock`/pthread ordering — rules still apply (CON35-C), but the examples here are C++.

## What the agent often gets wrong

- "Each thread's locking looks fine in isolation." ABBA deadlock is a property of the
  CYCLE across threads; you must enumerate every pair of lock sites, not one function.
- "std::lock_guard protects me." `std::lock_guard` only unlocks on scope exit; it does NOT
  prevent a deadlock when the two `lock()` calls it wraps are acquired in conflicting orders.
- "std::scoped_lock still needs ordering discipline." `std::scoped_lock(m1, m2)` uses
  `std::lock`'s deadlock-avoidance internally; conflicting order across threads is safe.
  Manually chained `lock_guard` constructors are NOT the same thing.
- "It worked in tests, so no deadlock." Deadlock needs a specific interleaving; the right
  way to prove it is a watchdog timeout, TSan, or helgrind — not a lucky run.
- "try_lock never blocks, so it's safe." Ignoring a `try_lock` failure (or looping on it)
  either enters a critical section without the lock (data race) or busy-waits.
- Reaching for `std::recursive_mutex` to silence a same-thread re-lock hang instead of
  fixing the re-entrant call structure.

## How to reason correctly

1. Model every lock site as an edge in the "holds → requests" graph; a deadlock exists iff a
   cycle forms and no participant can release. Check ALL threads, not one function.
2. Establish a single global lock order (CON35-C): every mutex gets a rank; acquire strictly
   ascending. When unsure whether two sites agree, find the second site that touches the
   same two mutexes in the opposite order.
3. For acquiring several mutexes atomically, use `std::lock` / `std::scoped_lock` — the
   ordering problem is solved by construction inside the call. Do not hand-roll sequences of
   `lock()`.
4. If a function must re-enter a locked region, refactor to `fn_locked()` helpers or an
   "already locked" parameter; prefer that over `std::recursive_mutex`.
5. Treat deadlock like a data race for verification: bounded watchdog for the deterministic
   demo, TSan/helgrind in CI to catch the order problem without waiting for the interleaving.

## What to verify

- Every multi-lock site in the program agrees on the order of the shared mutexes (rank table).
- `try_lock` failure paths release every lock acquired so far and never fall through into a
  "protected" section.
- Same-thread re-lock uses `std::recursive_mutex` only when recursion is genuinely required.
- Multi-mutex acquisition goes through `std::lock`/`std::scoped_lock`, not sequential `lock()`.
- The bad example deterministically deadlocks (watchdog fires), the good examples terminate.

## How to verify

```
g++ -std=c++17 -Wall -Wextra -Werror -O2 -pthread examples/bad/deadlock_bad.cpp -o out && ./out
# exit 42, "WATCHDOG: ABBA DEADLOCK" — expected failure (bounded by the watchdog)
g++ -std=c++17 -Wall -Wextra -Werror -O2 -pthread examples/good/deadlock_good.cpp -o out && ./out
g++ -std=c++17 -Wall -Wextra -Werror -O2 -pthread examples/good/deadlock_scoped_lock.cpp -o out && ./out
# both exit 0, "count=10"
# dynamic detection where TSan is available (Linux/macOS; not MinGW):
g++ -std=c++17 -fsanitize=thread -O1 -pthread examples/bad/deadlock_bad.cpp -o out && ./out
valgrind --tool=helgrind ./out
```

## Where the knowledge comes from

- ISO C++20 N4861 [thread.mutex.requirement], [thread.lock.algorithm], [thread.lock.scoped]
  (`iso-cpp20-n4861`)
- SEI CERT C CON35-C — lock in a predefined order; CON32-C/36-C (`cert-c`)
- C++ Core Guidelines CP.21 (use std::lock/scoped_lock), CP.50 (`cpp-core-guidelines`)
- C11 N1570 §7.26.4 (`iso-c11-n1570`); Clang/GCC docs on TSan deadlock detection
  (`clang-docs`, `gcc-manual`)

## Related skills

- `concurrency-condvar-and-spurious-wakeup` — the sibling failure mode for waits
- `memory-ordering-reasoning` — lock-free/atomic alternative to mutexes
- `sanitizer-report-reading` — interpreting TSan lock-order-inversion reports
- `c-undefined-behavior` — a data race that survives a lock mis-design is UB

## Evaluation

Synthetic: two threads locking A,B vs B,A (must flag, fix to a single order); `scoped_lock`
vs chained `lock_guard` (must prefer the former); `try_lock` with an ignored failure path.
Adversarial: a deadlock that only reproduces under a specific sleep/interleave, or a
"lock_guard fixed it" claim — the agent must use the watchdog/TSan evidence and the rank
table. False-positive: two threads using `scoped_lock(A, B)` in opposite argument order must
NOT be flagged (safe by construction); correct `try_lock` with a busy-return path must NOT be
flagged. Detection: name the cycle and the violated order; fix: single global order or
`scoped_lock`.
