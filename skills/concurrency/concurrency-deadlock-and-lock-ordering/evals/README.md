# Evaluation — concurrency-deadlock-and-lock-ordering

Skill: `skills/concurrency/concurrency-deadlock-and-lock-ordering`. Stability target: `evaluated`.

## Adversarial evals (core)

- **AD-01 (deterministic hang)**: two threads locking `A,B` and `B,A` with staggered sleeps —
  the bad example. Agent must: identify the ABBA cycle from BOTH lock sites, not one; explain
  why "it worked in my test" is irrelevant; produce the rank-order fix or `scoped_lock`.
- **AD-02 (false sense of RAII)**: chained `std::lock_guard l1(A); std::lock_guard l2(B);` in
  conflicting order — agent must NOT claim RAII "fixed" the deadlock; the fix is the same order
  in both threads or a single `std::scoped_lock`.
- **AD-03 (try_lock fallthrough)**: `if (!l2.try_lock()) { /* continue anyway */ }` — must flag
  the unprotected critical section (data race) and the missing unlock on the failure path.

## Synthetic evals

- **easy/negative**: two threads locking the same single mutex — must NOT be flagged.
- **medium/negative**: `scoped_lock(A, B)` vs `scoped_lock(B, A)` in two threads — must NOT be
  flagged (safe by construction; `std::lock` avoids the cycle).
- **hard/negative**: lock hierarchy violation across a 3-deep call chain (inode → mm vs
  mm → inode) — must flag via the rank table, not just the direct pair.
- **ambiguous**: `std::recursive_mutex` used for a self-recursive tree visitor — correct use,
  must NOT be flagged; but a recursive_mutex added to hide re-entrant `lock()` calls must be.

## False-positive evals

- Correct global lock order (both threads lock A then B) — must NOT be flagged.
- `std::scoped_lock(A, B)` in opposite argument order across threads — must NOT be flagged.
- `try_lock` with a proper "return busy" failure path releasing all held locks — must NOT be flagged.

## Verification fixtures

- `examples/bad/deadlock_bad.cpp` — ABBA pair; watchdog fires, exit 42 (bounded, no infinite hang).
- `examples/good/deadlock_good.cpp` — consistent order; count=10, exit 0.
- `examples/good/deadlock_scoped_lock.cpp` — `scoped_lock` multi-acquire; count=10, exit 0.

Commands (MinGW g++ 16.1, C++17 threads):
```
g++ -std=c++17 -Wall -Wextra -Werror -O2 -pthread examples/bad/deadlock_bad.cpp -o out && ./out   # exit 42
g++ -std=c++17 -Wall -Wextra -Werror -O2 -pthread examples/good/deadlock_good.cpp -o out && ./out # exit 0
g++ -std=c++17 -Wall -Wextra -Werror -O2 -pthread examples/good/deadlock_scoped_lock.cpp -o out && ./out # exit 0
```
Recorded 2026-08-14 on Windows/MinGW: bad = "WATCHDOG: ABBA DEADLOCK", exit 42; both good
examples exit 0. TSan (`-fsanitize=thread`) and helgrind are not available on this MinGW
toolchain; on Linux/macOS they additionally report "lock-order-inversion" / "Lock order violated".

## Scoring

- detection: names the cycle and both lock sites; never "it's just two lock calls".
- reasoning: uses the holds→requests graph / rank table, not per-function inspection.
- fix: single global order or `scoped_lock`; does not add a sleep to "make it pass".
- verification: cites the watchdog exit code or TSan/helgrind evidence.
