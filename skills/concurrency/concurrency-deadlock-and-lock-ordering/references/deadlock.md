# Deadlock & Lock Ordering — Reference

Sources (registry ids): `iso-cpp20-n4861` ([thread.mutex.requirement], [thread.lock.algorithm],
[thread.lock.scoped], [thread.recursive.mutex], [thread.timedmutex.requirement]),
`iso-c11-n1570` (§7.26.4 mtx_lock / §7.26.4.7 mtx_trylock), `cert-c` (CON35-C, CON36-C, CON32-C),
`cpp-core-guidelines` (CP.21, CP.42, CP.50), `clang-docs` / `gcc-manual` (TSan deadlock detection).

## 1. ABBA deadlock

- **RULE**: a cycle of two threads each holding one lock and waiting for the other — T1 holds A
  waits for B, T2 holds B waits for A — can never be broken; both block forever.
- **WHY AI GETS IT WRONG**: writes each thread's lock sequence in isolation and never checks that
  the set of orders across threads forms a cycle. "It worked in my test" because the cycle needs
  the interleaving to coincide; a deadlock is a scheduling accident waiting for the right moment.
- **CORRECT REASONING**: model lock acquisition as a graph: nodes = mutexes, directed edges from
  an already-held mutex to a requested one. A deadlock exists iff the graph has a cycle and no
  thread can release. Check ALL threads, not one.
- **EXAMPLE** (bad): `worker1` does `A.lock(); B.lock();` while `worker2` does `B.lock(); A.lock();`
  (see `examples/bad/deadlock_bad.cpp` — watchdog reports ABBA DEADLOCK, exit 42).
- **COUNTEREXAMPLE** (good): both workers acquire in the same global order `A` then `B`
  (see `examples/good/deadlock_good.cpp` — terminates, exit 0).
- **VERIFICATION**: run both examples; bad must hang and be caught by the bounded watchdog, good
  must terminate. TSan reports "lock-order-inversion (potential deadlock)" at runtime.
- **SOURCE**: cert-c CON35-C; iso-cpp20-n4861 [thread.mutex.requirement].

## 2. Lock ordering discipline (CON35-C)

- **RULE**: assign a global, documentation-visible total order to every mutex (rank 0, 1, 2, ...)
  and always acquire lower-rank locks before higher-rank ones. A consistent total order makes a
  hold-and-wait cycle impossible.
- **WHY AI GETS IT WRONG**: orders locks per-function "naturally" (e.g. parameter order, or the
  order they were declared in the same struct) and the orders disagree between functions.
- **CORRECT REASONING**: the order is a property of the whole program, not of a single function.
  Two functions locking the same two mutexes in different orders is already a latent deadlock.
  Centralize: rank each mutex once, derive a lock-ordering table, and check every site against it.
- **EXAMPLE** (bad): `transfer(accA, accB)` locks `accA` then `accB`, while `transfer(accB, accA)`
  called from another thread locks them in the reverse order → ABBA.
- **COUNTEREXAMPLE** (good): always lock accounts by ascending address / rank:
  `if (accA < accB) { lock A; lock B; } else { lock B; lock A; }`.
- **VERIFICATION**: clang-tidy `cert-con35-c`; TSan lock-order-inversion; helgrind "Lock order
  violated"; code review against the rank table.
- **SOURCE**: cert-c CON35-C.

## 3. `std::lock` / `std::scoped_lock` for multiple mutexes

- **RULE**: to lock several mutexes atomically, use `std::lock(m1, m2, ...)` or
  `std::scoped_lock(m1, m2, ...)` (C++17). The library uses a deadlock-avoidance algorithm
  (lock one, `try_lock` the rest, back off and retry) so acquisition never deadlocks regardless
  of the argument order in different threads.
- **WHY AI GETS IT WRONG**: believes "std::scoped_lock still needs my ordering care" or that
  chaining `std::lock_guard` constructors `lock_guard a(m1); lock_guard b(m2);` is equivalent.
  It is not: sequential `lock()` calls are the same as manual locking and can ABBA-deadlock.
- **CORRECT REASONING**: `std::lock` provides the ordering for you by construction — inside a
  single call. The RAII wrappers must be constructed FROM the already-locked state, e.g.
  `std::lock(m1, m2); std::lock_guard la(m1, std::adopt_lock); ...` or simply `std::scoped_lock l(m1, m2);`.
  `std::scoped_lock` (C++17) is `lock_guard` semantics + `std::lock` acquisition.
- **EXAMPLE** (bad): `std::lock_guard l1(A); std::lock_guard l2(B);` in thread 1 and
  `std::lock_guard l1(B); std::lock_guard l2(A);` in thread 2 — identical to the manual ABBA case.
- **COUNTEREXAMPLE** (good): `std::scoped_lock lock(A, B);` in both threads (see
  `examples/good/deadlock_scoped_lock.cpp` — terminates, exit 0).
- **VERIFICATION**: compile/run both; the scoped_lock version always terminates.
- **SOURCE**: iso-cpp20-n4861 [thread.lock.algorithm], [thread.lock.scoped]; cpp-core-guidelines CP.21.

## 4. Lock hierarchy

- **RULE**: give each mutex a rank; a thread may only acquire a mutex whose rank is strictly
  greater than every mutex it currently holds. This generalizes order discipline to the whole
  call tree (e.g. driver layers, DBMS latches).
- **WHY AI GETS IT WRONG**: thinks "predefined order" only matters between two direct siblings;
  a nested call chain `f → g → h` can lock `L1, L2, L3` while another path locks `L1, L3, L2`.
- **CORRECT REASONING**: ranks make violations machine-checkable (assert the rank decreases at
  every acquire in debug builds). If the hierarchy is broken, the fix is structural (split locks,
  coarse-grained locking), not adding more ordering rules.
- **EXAMPLE** (bad): a kernel path locks inode mutex then mmap lock; a second path (mmap → page
  fault → inode) inverts the order and the machine deadlocks under memory pressure.
- **COUNTEREXAMPLE** (good): document ranks (INODE=1 < MM=2), acquire strictly ascending, assert
  in debug: `ASSERT(rank(new) < rank(held))` per acquire.
- **VERIFICATION**: debug-rank assertions; lockdep (Linux kernel) — this is exactly what lockdep
  detects; TSan on the userspace analogue.
- **SOURCE**: cpp-core-guidelines CP.50 (define mutex + guarded data together); cert-c CON35-C.

## 5. Recursive locking

- **RULE**: locking a plain `std::mutex` twice from the same thread is a programming error
  (deadlock or UB); use `std::recursive_mutex` only when a function may legitimately re-enter its
  own critical section. Recursive mutexes add a per-thread ownership count and hide re-entrancy
  problems.
- **WHY AI GETS IT WRONG**: greps that a function "sometimes" calls a locked helper and reaches
  for `recursive_mutex` to silence the hang, instead of fixing the re-entrant call structure.
- **CORRECT REASONING**: a recursive mutex only fixes same-thread re-acquisition. It does NOT help
  cross-thread ABBA, and it makes every lock call slightly slower and the invariant "no reentry"
  unverifiable. Refactor so the inner function takes a "already locked" flag or a lock-state
  object; use `std::recursive_mutex` only for genuinely recursive algorithms over a self-referential
  structure (e.g. tree traversal calling a locked visitor).
- **EXAMPLE** (bad): `void put() { lock(); if (flush) put(); unlock(); }` — second `put` deadlocks
  on the same `std::mutex`.
- **COUNTEREXAMPLE** (good): split the locked body into `put_locked()` and have `put()` acquire the
  mutex once, or make the recursion call the unlocked internal variant.
- **VERIFICATION**: the good refactor compiles and terminates; TSan/helgrind do not flag it.
- **SOURCE**: iso-cpp20-n4861 [thread.recursive.mutex]; cert-c CON35-C.

## 6. `try_lock` and deadlock avoidance

- **RULE**: `try_lock` returns `false` on contention; the caller MUST handle failure. Ignoring the
  result and continuing into the critical section is a data race. Acquiring several mutexes with
  per-mutex `try_lock` can still deadlock: T1 `try_lock(A)` then spins on B while T2 holds B.
- **WHY AI GETS IT WRONG**: uses `try_lock` "so we never block", then proceeds anyway, or loops
  `while(!b.try_lock());` — a busy-wait that is a livelock/deadlock risk with a CPU spin.
- **CORRECT REASONING**: `try_lock` is for the "if I can't get it now, do something else" pattern,
  never for "try, then continue anyway". For multi-lock, prefer `std::lock`/`scoped_lock`; a
  `try_lock` retry loop should release everything it holds before backing off.
- **EXAMPLE** (bad): `if (l2.try_lock()) { /* use both */ } l1.unlock();` — the `else` path forgot
  `l1.unlock()`, or worse, used the data while holding only `l1`.
- **COUNTEREXAMPLE** (good): `if (!l1.try_lock()) return busy; if (!l2.try_lock()) { l1.unlock(); return busy; }`
  — every held lock is released on every path.
- **VERIFICATION**: TSan reports the unlocked/racy access; review all `try_lock` branches with a
  reachability check on the unlock count.
- **SOURCE**: iso-cpp20-n4861 [thread.timedmutex.requirement], [thread.mutex.requirement];
  iso-c11-n1570 §7.26.4.7 (mtx_trylock).

## 7. Detecting deadlock with TSan / helgrind

- **RULE**: deadlock detection is a runtime, whole-program analysis: TSan (`-fsanitize=thread`)
  reports lock-order-inversion when it observes a thread blocked on a mutex while holding one that
  participates in a cycle; valgrind's helgrind does the same for lock orders. Static review finds
  the cycle only if the agent enumerates every lock site.
- **WHY AI GETS IT WRONG**: relies on "it ran fine" or on a single-threaded trace; expects a
  deadlock to reproduce deterministically. It is a race in scheduling, so it may take thousands of
  runs.
- **CORRECT REASONING**: treat deadlock like a data race: make it deterministic via a watchdog
  timeout (see the examples), and add TSan/helgrind to CI because they catch the ORDER problem
  without needing the unlucky interleaving to occur in a plain run.
- **EXAMPLE** (bad): the ABBA pair in `examples/bad/deadlock_bad.cpp` — TSan reports
  "lock-order-inversion (potential deadlock)" the first time both threads hold their first lock.
- **COUNTEREXAMPLE** (good): `examples/good/deadlock_good.cpp` / `deadlock_scoped_lock.cpp` — TSan
  runs clean, process exits 0.
- **VERIFICATION**: `g++ -fsanitize=thread ...` (Linux/macOS; not supported by MinGW on Windows);
  `valgrind --tool=helgrind ./out`; on this repo's Windows/MinGW toolchain, the bounded watchdog +
  exit code is the executable check.
- **SOURCE**: clang-docs (ThreadSanitizer deadlock detection); gcc-manual (-fsanitize=thread);
  cert-c CON35-C.
