# Condition Variables & Spurious Wakeup — Reference

Sources (registry ids): `iso-cpp20-n4861` ([thread.condition.variable], [thread.condition.condvarany],
[thread.mutex.requirement]), `iso-c11-n1570` (§7.26.5 cnd_wait / cnd_signal / cnd_broadcast),
`cert-c` (CON36-C, CON35-C), `cpp-core-guidelines` (CP.21, CP.42, CP.50).

## 1. A condvar is meaningless without a predicate and a mutex

- **RULE**: `condition_variable` only blocks/unblocks threads; it carries no state. The "what am I
  waiting for?" state lives in a predicate (usually a bool/counter) protected by a mutex. Every
  `wait` must re-check that predicate, and every `notify` must follow a predicate update under the
  same mutex.
- **WHY AI GETS IT WRONG**: treats the condvar as a "signal flag" — `notify` = "something
  happened", `wait` = "block until notified" — and forgets the shared predicate entirely.
- **CORRECT REASONING**: the pattern is: producer locks the mutex, mutates the predicate, unlocks,
  notifies; consumer locks the mutex, waits on a loop over the predicate. The predicate is the
  source of truth; the notify is only a "check again" hint.
- **EXAMPLE** (bad): `cv.notify_one();` with no shared predicate and no mutex — a waiter that wakes
  cannot learn what changed and cannot safely re-check anything.
- **COUNTEREXAMPLE** (good): `{ lock_guard lk(m); ready = true; } cv.notify_one();` +
  `cv.wait(lk, [] { return ready; });` (see `examples/good/condvar_good.cpp`).
- **VERIFICATION**: remove the predicate and the "wait without predicate" version hangs (see
  `examples/bad/condvar_bad.cpp`); TSan on the racy predicate.
- **SOURCE**: cpp-core-guidelines CP.42 (don't wait without a condition); iso-cpp20-n4861
  [thread.condition.variable].

## 2. Spurious wakeups (CON36-C)

- **RULE**: `wait` may return even when nothing was notified (POSIX/C++/C11 all allow it — C11
  §7.26.5.4 says the wait ends "until ... spurious wakeup occurs"). The waiter MUST re-check the
  predicate in a loop; the caller of `cnd_wait`/`wait` must be wrapped in a loop that re-evaluates
  the condition.
- **WHY AI GETS IT WRONG**: assumes "the thread was notified, so the predicate is true" — a
  spurious return reads the predicate while it is still false and consumes a half-updated state.
- **CORRECT REASONING**: every wakeup is "maybe the predicate changed, go look" — never "the
  predicate is true". Encode that in the loop condition, not in an `if` after `wait`.
- **EXAMPLE** (bad): `cv.wait(lk); if (queue.empty()) continue; // bug — handles only this case`
  vs the correct `while (queue.empty()) cv.wait(lk);`.
- **COUNTEREXAMPLE** (good): `while (!ready) cv.wait(lk);` or the predicate overload
  `cv.wait(lk, [] { return ready; })`.
- **VERIFICATION**: stress the waiter with extra notifies; it must never observe a false predicate.
  The bad `cv.wait(lk)` (no loop) is flagged by code review; CON36-C is the normative reference.
- **SOURCE**: cert-c CON36-C; iso-c11-n1570 §7.26.5.4; iso-cpp20-n4861 [thread.condition.variable].

## 3. Lost wakeup

- **RULE**: if the producer mutates the predicate and notifies while the consumer is NOT yet in
  `wait`, the notification is lost: the consumer then blocks forever even though the predicate is
  already true. The mutex does not rescue you — `wait` atomically unlocks and blocks, and a notify
  that arrives before the consumer enters `wait` is gone.
- **WHY AI GETS IT WRONG**: reasons "I notify after setting the flag, that covers everything" and
  ignores the ordering between the notify and the consumer's entry into `wait`.
- **CORRECT REASONING**: the notification has no memory; only the predicate has state. A consumer
  that checks the predicate (in a loop / via the predicate overload) BEFORE blocking is immune: if
  the predicate is already true it never blocks at all. This is exactly why the predicate is
  mandatory.
- **EXAMPLE** (bad): producer sets `ready=true` and notifies; consumer then calls `cv.wait(lk)`
  (no predicate) — waits forever. Reproduced deterministically in `examples/bad/condvar_bad.cpp`
  (watchdog exit 42).
- **COUNTEREXAMPLE** (good): same timing, but `cv.wait(lk, [] { return ready; })` — the predicate
  is evaluated before sleeping, `ready` is already true, and the consumer proceeds immediately
  (see `examples/good/condvar_good.cpp`).
- **VERIFICATION**: run the bad/good pair; the bad one hangs (watchdog), the good one exits 0.
- **SOURCE**: cpp-core-guidelines CP.42; cert-c CON36-C.

## 4. `wait(lk, pred)` — the predicate overload

- **RULE**: `cv.wait(lk, pred)` is specified as `while (!pred()) cv.wait(lk);` — it re-checks the
  predicate under the held lock before blocking AND after every (including spurious) wakeup. It is
  the correct idiom for 95% of consumers.
- **WHY AI GETS IT WRONG**: writes the loop by hand and breaks it (checks predicate without the
  lock, or `if` instead of `while`), when the standard library already provides the correct loop.
- **CORRECT REASONING**: use the overload; it removes the two classic errors — predicate checked
  outside the lock, and predicate checked once instead of in a loop. The predicate reads the state
  while `lk` is held, which the mutex makes race-free against the producer.
- **EXAMPLE** (bad): `bool ok = !queue.empty(); cv.wait(lk); use(ok);` — stale read before wait.
- **COUNTEREXAMPLE** (good): `cv.wait(lk, [] { return !queue.empty(); }); use(queue.front());`.
- **VERIFICATION**: compile/run `examples/good/condvar_good.cpp`; the overload is a single call,
  reviewable at a glance.
- **SOURCE**: iso-cpp20-n4861 [thread.condition.variable.wait]; cert-c CON36-C.

## 5. `notify_one` vs `notify_all`

- **RULE**: `notify_one` wakes at most one waiter; `notify_all` wakes every waiter. If exactly one
  consumer can consume the produced state, `notify_one` is fine and cheaper. If several waiters may
  each need to re-check (multiple consumers of a single event, or waiters on DIFFERENT predicates
  sharing one condvar), `notify_all` is required — `notify_one` can starve a waiter forever.
- **WHY AI GETS IT WRONG**: "notify_one is an optimization" — and it is, only when the waiter count
  and predicate are proven to fit it. With multiple waiters, `notify_one` repeatedly waking the
  same thread while another waits on a different predicate = missed wakeup = hang.
- **CORRECT REASONING**: ask: "after this notify, which waiters are guaranteed to make progress?"
  One → `notify_one`. Several, or unknown/foreign waiters on the same condvar, or the predicate is
  broadcast-style → `notify_all`. Err toward `notify_all` when unsure; correctness first.
- **EXAMPLE** (bad): a barrier with `N` waiters and a producer calling `notify_one` N times in a
  tight loop — each wake rechecks a predicate that only the LAST one may satisfy, or the same
  thread wins every round; others hang.
- **COUNTEREXAMPLE** (good): the barrier producer calls `notify_all` after setting the release
  predicate; every waiter wakes, re-checks, proceeds.
- **VERIFICATION**: stress with N waiters; the `notify_one` version hangs under load, the
  `notify_all` version always terminates.
- **SOURCE**: iso-cpp20-n4861 [thread.condition.variable.notify]; cert-c CON36-C.

## 6. Signaling before vs after releasing the lock

- **RULE**: correctness-wise it does not matter: the notified thread still must re-acquire the
  mutex, and the predicate is only read under the lock. Performance-wise, notifying while holding
  the lock can make the woken thread immediately block on the mutex (extra context switch /
  thundering herd). The standard best practice is: update the predicate under the lock, release
  the lock, then notify.
- **WHY AI GETS IT WRONG**: claims "you MUST notify while holding the lock" (or "you MUST release
  first") as a correctness rule, or notifies before updating the predicate.
- **CORRECT REASONING**: the ordering that matters is: predicate mutation is sequenced-before the
  notify in the SAME thread, and the waiter re-reads the predicate after acquiring the lock in the
  OTHER thread. Notify-with-lock-held is correct but slower; notify-before-predicate-update is
  wrong (a waiter may wake and read the stale predicate; then it waits again — correct only because
  of the loop, but it is a wasted wakeup).
- **EXAMPLE** (bad): `cv.notify_one(); { lock lk; ready = true; }` — the waiter that wakes first
  reads `ready == false` and re-blocks (works only because of the loop; wasted wakeup, and with a
  lost-wakeup-prone `wait` it deadlocks).
- **COUNTEREXAMPLE** (good): `{ lock lk; ready = true; } cv.notify_one();` — predicate visible to
  the waiter the moment it acquires the lock, no redundant wakeup.
- **VERIFICATION**: run both; the bad timing combined with a no-predicate wait hangs; the good
  order always terminates.
- **SOURCE**: cpp-core-guidelines CP.21/CP.50; iso-cpp20-n4861 [thread.condition.variable].

## Quick decision table

| Situation | Correct choice |
|---|---|
| any consumer | `cv.wait(lk, pred)` predicate overload |
| exactly one consumer | `notify_one` |
| multiple consumers / shared condvar | `notify_all` |
| predicate update + notify | update under lock, unlock, then notify |
| waiting "for a signal" with no state | redesign: add the predicate |
