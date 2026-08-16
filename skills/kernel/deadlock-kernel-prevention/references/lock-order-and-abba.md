# Kernel Deadlock Prevention: Lock Ordering and AB-BA

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE →
COUNTEREXAMPLE → VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED /
UNVERIFIED.

## 1. AB-BA: two sites acquiring locks in opposite orders are a deadlock

- **RULE**: if site X acquires A then B, and site Y acquires B then A, the
  two locks can be held in a cycle (each waits on the lock the other holds).
  The deadlock is independent of timing — it exists whenever the two sites
  can run concurrently. KNOWN (classic deadlock theory; lockdep-design).
- **WHY AI GETS IT WRONG**: the two sites are often far apart or in
  different files; agents review locally and miss the cross-site cycle,
  then "prove safety" from one run that never hit the interleaving.
- **CORRECT REASONING**: build the dependency graph over ALL acquisition
  sites; a cycle of held-then-acquired edges is a deadlock class. A single
  completed run proves nothing — the cycle needs the right interleaving,
  which lockdep finds from any partial execution.
- **EXAMPLE** (bad): `examples/bad/abba_deadlock_pthread.c` — thread 1 takes
  A then B; thread 2 takes B then A; a watchdog reports the hang.
- **COUNTEREXAMPLE** (good): `examples/good/lock_order_pthread.c` — both
  threads take locks in the same global order (A before B); no cycle,
  completes.
- **VERIFICATION**: the good fixture exits 0; the bad fixture exits 2 with
  "DEADLOCK DETECTED" via its watchdog. Recorded on this host.
- **SOURCE**: kernel-lockdep-docs (Multi-lock dependency rules) [proposed];
  posix-threads (fixture semantics).

## 2. Lock ordering is a global invariant, not a local one

- **RULE**: the ordering rule ("lock A is always acquired before B") must
  hold for every site in the whole subsystem/whole kernel interaction; a
  single violation anywhere reintroduces the cycle. The lockdep closure
  proof means every simple chain that ever occurred lets the validator prove
  no combination can deadlock. KNOWN (lockdep-design, "Proof of 100%
  correctness").
- **WHY AI GETS IT WRONG**: the agent fixes only the splat's two sites and
  misses a third site with the opposite order.
- **CORRECT REASONING**: treat the reported cycle as one edge of a global
  graph; scan all callers of both locks for order violations, then fix the
  order at its root.
- **EXAMPLE** (bad): fixing the reported A->B/B->A pair while a third
  function still does B->A.
- **COUNTEREXAMPLE** (good): the subsystem adopts one order everywhere; the
  lockdep graph stays acyclic.
- **VERIFICATION**: `python examples/good/lockdep_cycle_detect.py` finds the
  cycle from a graph that includes the third site; the bad model misses it.
- **SOURCE**: kernel-lockdep-docs (Proof of closure; Lock-class) [proposed].

## 3. _nested() encodes a real hierarchy; misusing it hides deadlocks

- **RULE**: `mutex_lock_nested(L, SUBCLASS)` tells lockdep to treat the
  acquisition as a distinct subclass of a hierarchical order (e.g. whole
  disk vs partition). It is valid only where a static, documented hierarchy
  exists. Applying it to an arbitrary order removes the cycle from the
  graph — a false negative that the agent itself created. KNOWN
  (lockdep-design, "Exception: nested data dependencies").
- **WHY AI GETS IT WRONG**: `_nested()` is used as "make lockdep shut up",
  which converts a real deadlock into an undetectable one.
- **CORRECT REASONING**: only use subclassing for object hierarchies with a
  provable natural order; otherwise change the acquisition order.
- **EXAMPLE** (bad): a random two-lock function switched to
  `mutex_lock_nested` with an invented subclass to silence the splat.
- **COUNTEREXAMPLE** (good): `examples/good/nested_lock_ordering.c` models
  the whole/partition hierarchy: the subclass is derived from the object
  type, and every acquisition follows it.
- **VERIFICATION**: host fixture runs; on target, dropping the hierarchy
  must restore the splat (documented).
- **SOURCE**: kernel-lockdep-docs (Nested locking) [proposed].

## 4. Sleep while holding a spinlock is both a bug and a deadlock vector

- **RULE**: spinlocks disable preemption (and, depending on variant, IRQs);
  any call that can sleep (kmalloc GFP_KERNEL, mutex, copy_to_user,
  down_read) while a spinlock is held triggers
  `sleeping function called from invalid context` and can deadlock the
  whole CPU. KNOWN (kernel practice; DEBUG_ATOMIC_SLEEP).
- **WHY AI GETS IT WRONG**: the sleep call is hidden inside a helper; the
  agent reviews the visible call only and misses the transitive sleep.
- **CORRECT REASONING**: walk the call tree under every spinlock-held
  region and classify each callee as sleep-capable or not.
- **EXAMPLE** (bad): `examples/bad/sleep_under_lock.c` — "kmalloc(GFP_KERNEL)"
  under the lock (modeled by a blocking call).
- **COUNTEREXAMPLE** (good): the allocation happens before taking the lock
  (or uses the GFP_ATOMIC analog).
- **VERIFICATION**: `gcc ... bad/sleep_under_lock.c && ./d4.exe` prints the
  splat shape; the corrected pattern is in the good fixtures' comments.
- **SOURCE**: ldd3 (ch.5 atomic context); kernel-source
  (DEBUG_ATOMIC_SLEEP); kernel-lockdep-docs [proposed].
