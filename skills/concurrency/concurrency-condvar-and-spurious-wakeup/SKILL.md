---
name: concurrency-condvar-and-spurious-wakeup
description: Use when writing, reviewing, or debugging condition-variable code (std::condition_variable, C11 cnd_*) — pairing wait with a predicate and mutex, handling spurious and lost wakeups (CON36-C), choosing notify_one vs notify_all, or fixing a program that occasionally hangs.
---

# Condition Variables & Spurious Wakeup

## When to use

- Writing a producer/consumer or one-shot-flag wait with `std::condition_variable` (or C11 `cnd_*`).
- Debugging a program that "sometimes" blocks forever at a `wait` call.
- Deciding between `wait(lk)` and `wait(lk, pred)`, or between `notify_one` and `notify_all`.
- Reviewing whether the predicate is guarded by the same mutex as the condvar.

## When not to use

- Acquiring several mutexes at once — use `concurrency-deadlock-and-lock-ordering`.
- Spin-wait on an atomic flag where a blocking wait is undesirable — consider
  `memory-ordering-reasoning` / atomics first.
- Signaling from an ISR or signal handler (async-signal-safety, futex semantics) — out of scope.
- Rust `Condvar`/`Mutex` poisoning interactions — Rust's guard poison is `rust-panic-safety` territory.

## What the agent often gets wrong

- "`wait` returns when notified, so the condition is true." Spurious wakeups are legal;
  `wait` returning means only "go check the predicate again". C11 §7.26.5.4 names them explicitly.
- "I notified, so it can't be lost." A notify that arrives before the consumer enters `wait`
  is gone forever; only the predicate has memory. `wait(lk)` with no predicate then blocks
  forever even though the predicate is already true.
- "`if` around wait is enough." It must be a LOOP (or the predicate overload, which loops);
  `if` re-checks once and still misbehaves on the second spurious wakeup.
- "`notify_one` is always a safe optimization." With multiple waiters (or waiters on different
  predicates sharing the condvar), `notify_one` can starve a waiter — missed wakeup.
- "The predicate can be checked without the mutex." It must be read under the same lock the
  producer writes under; otherwise it is a data race (UB).
- "I must notify while holding the lock" (or "must not"). Both are correct; the rule is only
  that the predicate update happens under the lock and is sequenced-before the notify.

## How to reason correctly

1. State the invariant explicitly: the predicate (bool/count/queue-nonempty) is protected by
   the mutex `m`; the condvar `cv` is only the block/unblock mechanism. State lives in the
   predicate, not the condvar.
2. Consumer: acquire `m`, then `cv.wait(lk, pred)` — the predicate overload is literally
   `while (!pred()) cv.wait(lk);` and re-checks before blocking and after every wakeup.
3. Producer: acquire `m`, mutate the predicate, release `m`, then `notify` (notify after
   unlock avoids a needless handoff of the mutex).
4. Choose `notify_one` only when exactly one waiter can make progress; otherwise `notify_all`.
5. To prove the lost-wakeup fix: reason about the "notify before consumer enters wait"
   interleaving — the predicate check at the top of `wait(lk, pred)` is what saves it.

## What to verify

- Every `wait` is the predicate overload (or an explicit `while` loop over the predicate).
- Predicate reads happen while the mutex is held and match the producer's write-side lock.
- Producer order: mutate predicate under lock → unlock → notify (never notify before mutate).
- `notify_all` is used whenever more than one waiter may need to re-check.
- The bad example hangs deterministically (watchdog exit 42), the good example exits 0.

## How to verify

```
g++ -std=c++17 -Wall -Wextra -Werror -O2 -pthread examples/bad/condvar_bad.cpp -o out && ./out
# exit 42, "WATCHDOG: consumer still waiting => LOST WAKEUP" — expected failure (bounded)
g++ -std=c++17 -Wall -Wextra -Werror -O2 -pthread examples/good/condvar_good.cpp -o out && ./out
# exit 0 — the same lost-wakeup timing is handled by the predicate overload
# data-race check of the predicate (where TSan is available; not MinGW):
g++ -std=c++17 -fsanitize=thread -O1 -pthread examples/good/condvar_good.cpp -o out && ./out
```

## Where the knowledge comes from

- ISO C++20 N4861 [thread.condition.variable] (`iso-cpp20-n4861`)
- SEI CERT C CON36-C — wrap wait in a loop for spurious wakeup; CON35-C (`cert-c`)
- C++ Core Guidelines CP.42 — don't wait without a condition (`cpp-core-guidelines`)
- C11 N1570 §7.26.5 — `cnd_wait` "until spurious wakeup occurs" (`iso-c11-n1570`)

## Related skills

- `concurrency-deadlock-and-lock-ordering` — lock ordering for the mutex protecting the predicate
- `memory-ordering-reasoning` — atomics/flags as an alternative to condvar signaling
- `sanitizer-report-reading` — TSan on racy predicates
- `rust-panic-safety` — Rust mutex poisoning vs condvar waits

## Evaluation

Synthetic: `wait` without a predicate (must flag, fix to predicate loop/overload); lost-wakeup
timing (notify before wait) with a no-predicate wait (must flag); `notify_one` with N waiters
(must flag to `notify_all`). Adversarial: a producer that sometimes signals under the lock and
sometimes after — must explain both are correct but the predicate must be updated first; a
"wait never hangs in my test" claim — must use the bounded watchdog/TSan evidence. Detection:
name the missing predicate/mutex pair. Fix: predicate overload + notify after unlock.
False-positive: correct `wait(lk, pred)` + `notify_all` one-shot and N-consumer cases must NOT
be flagged.
