# Evaluation — concurrency-condvar-and-spurious-wakeup

Skill: `skills/concurrency/concurrency-condvar-and-spurious-wakeup`. Stability target: `evaluated`.

## Adversarial evals (core)

- **AD-01 (lost wakeup)**: producer sets the predicate and notifies before the consumer ever
  reaches `wait`; consumer uses `cv.wait(lk)` with no predicate and blocks forever. Agent must:
  explain why the notify is lost, that only the predicate has memory, and fix to
  `cv.wait(lk, [] { return ready; })`.
- **AD-02 (spurious wakeup)**: consumer does `cv.wait(lk); if (ready) {...}` — a spurious
  wakeup before the producer runs reads `ready == false` and the "consumed" path is skipped.
  Agent must convert `if` to a loop / predicate overload and cite CON36-C.
- **AD-03 (notify_one starvation)**: a barrier with N waiters and `notify_one` — must flag to
  `notify_all`; repeated `notify_one` can wake the same thread while others hang.

## Synthetic evals

- **easy/negative**: single consumer + `notify_one` with `wait(lk, pred)` — correct, not flagged.
- **medium/negative**: wait without a predicate loop — must flag.
- **hard/negative**: notify before the predicate update (`cv.notify_one(); { lock; ready = true; }`)
  — must flag the wasted/raced wakeup (works only by luck of the loop).
- **ambiguous**: notify while holding the lock vs after releasing — both correct; the agent must
  not treat either as a bug, but should prefer notify-after-unlock for performance.

## False-positive evals

- `cv.wait(lk, pred)` with `notify_all` one-shot — correct, not flagged.
- N consumers + `notify_all` broadcast — correct, not flagged.
- notify after unlock (predicate updated under lock first) — correct, not flagged.

## Verification fixtures

- `examples/bad/condvar_bad.cpp` — no-predicate `wait`; deterministic lost wakeup; watchdog
  exit 42 (bounded, no infinite hang).
- `examples/good/condvar_good.cpp` — same timing but `wait(lk, pred)`; the predicate is checked
  before blocking, the consumer proceeds; exit 0.

Commands (MinGW g++ 16.1, C++17 threads):
```
g++ -std=c++17 -Wall -Wextra -Werror -O2 -pthread examples/bad/condvar_bad.cpp -o out && ./out  # exit 42
g++ -std=c++17 -Wall -Wextra -Werror -O2 -pthread examples/good/condvar_good.cpp -o out && ./out # exit 0
```
Recorded 2026-08-14 on Windows/MinGW: bad = "WATCHDOG: consumer still waiting => LOST WAKEUP",
exit 42; good prints "consumer: woke with ready=true" and exits 0. TSan on the predicate is
unavailable on this MinGW toolchain (Linux/macOS: `-fsanitize=thread`).

## Scoring

- detection: names the missing predicate / lost wakeup / spurious wakeup mechanism.
- reasoning: distinguishes "notified" from "predicate true"; cites CON36-C / CP.42.
- fix: predicate overload (or explicit loop) + predicate updated under the lock before notify.
- verification: cites the watchdog exit code or TSan evidence.
