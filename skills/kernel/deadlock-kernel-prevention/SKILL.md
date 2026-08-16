---
name: deadlock-kernel-prevention
description: Use when reviewing kernel locking, fixing lockdep splats, or designing lock ordering. Teaches AB-BA lock inversion, irq-context (irq-safe/unsafe) rules, sleeping-while-locked violations, nested locking, and how lockdep proves classes of deadlocks.
---

# Kernel Deadlock Prevention (lockdep & Lock Ordering)

## When to use

- Reviewing kernel code that acquires more than one lock, or a lock in a
  context where IRQs can fire.
- A lockdep splat ("possible circular locking dependency detected",
  "inconsistent lock state") needs a real fix, not a suppression.
- Designing lock ordering for a new subsystem: choosing a global order,
  using `_nested()` subclassing, and annotating with
  `lockdep_assert_held*`/`lockdep_*pin_lock`.
- Deciding whether a function may sleep given its lock context (spinlock
  held, IRQ disabled, RCU read-side, preempt disabled).

## When not to use

- User-space pthread mutex deadlocks and lock-free programming — use
  `concurrency-deadlock-and-lock-ordering` (C11/pthread model).
- Data races (two accesses without synchronization) — use
  `data-race-kernel-detection`.
- Ordering/barrier design without lock acquisition — use
  `kernel-rcu-memory-barriers`.
- Writing the locking subsystem itself — that is kernel-internal design
  beyond a review skill.

## What the agent often gets wrong

- Fixes a lockdep splat by adding `lockdep_off()`, `nested` keywords
  without a real hierarchy, or `#ifdef CONFIG_DEBUG_LOCKDEP` suppression.
  Lockdep is not the bug; suppressing it certifies the deadlock.
- Thinks "it didn't deadlock in my test" means "it cannot deadlock".
  lockdep proves a *class* of deadlocks from single executions: it needs
  each simple lock chain to occur once, then it proves no combination can
  deadlock. The absence of a lockdep splat on the tested path is not the
  same as lockdep having seen the path.
- Misses AB-BA inversion because the two lock sites are far apart or in
  different subsystems. The ordering rule is global: if A is ever taken
  before B anywhere, B-before-A is illegal everywhere.
- Ignores IRQ context: a lock taken in both hardirq and process context
  must be taken with IRQs disabled in the process context (`spin_lock_irq`),
  or an interrupt can self-deadlock. "irq-safe vs irq-unsafe" is a lock
  property lockdep tracks and the agent must state.
- Treats `mutex_lock_nested` as "tell lockdep it's fine". It only teaches
  the validator a hierarchy (subclass). Using it on a non-hierarchical
  ordering hides a real cycle — which lockdep then cannot see (false
  negative created by the agent).
- Forgets that sleepable operations (`kmalloc(GFP_KERNEL)`, mutex,
  copy_to_user) inside spinlocks are both a performance bug and a deadlock
  vector (`sleeping function called from invalid context`).
- Uses `lockdep_assert_held` as decoration; it is a runtime WARN gate that
  converts "assume the lock is held" into a checked requirement.

## How to reason correctly

1. Enumerate every lock acquisition in the reviewed code and build the
   dependency edges L1 -> L2 (L1 held when L2 acquired).
2. Establish a single global ordering rule and verify no site violates it;
   any violation is an AB-BA deadlock by construction.
3. Classify each lock's context: hardirq, softirq, process. A lock acquired
   in an IRQ context must be irq-disabling in lower contexts
   (`spin_lock_irq*`), and irq-safe locks must never be held while acquiring
   an irq-unsafe lock.
4. Classify sleep: enumerate the lock contexts that forbid sleeping
   (spinlock/raw spinlock, RCU read-side, preempt disabled, IRQ) and check
   every call inside them against the sleep list.
5. Use `mutex_lock_nested` only for a real static hierarchy (subclass),
   documented with the ordering invariant; else keep a global order.
6. Add `lockdep_assert_held*` and `lockdep_pin_lock` where an invariant
   depends on a held lock; run CONFIG_PROVE_LOCKING builds and let the
   splats teach the remaining dependencies.

## What to verify

- No lock is acquired in both orders anywhere in the reviewed code (the
  AB-BA scan covers all sites, not just the reported one).
- Every IRQ-context acquisition is paired with irq-disabled acquisition in
  lower contexts; the irq-safe/irq-unsafe matrix is stated.
- No sleepable call sits under a spinlock/RCU-read/preempt-disabled region;
  `GFP_ATOMIC` or pre-allocation used where needed.
- `_nested()`/subclass usage corresponds to a documented, static hierarchy.
- `lockdep_assert_held*` annotations are present where the code depends on
  a held lock, and `CONFIG_PROVE_LOCKING` builds are clean on the paths
  exercised.

## How to verify

Host-side (pthread simulation of the lock-order logic; no kernel here):

```
gcc -Wall -Wextra -Werror -O2 -pthread examples/good/lock_order_pthread.c -o /tmp/d1.exe && /tmp/d1.exe
gcc -Wall -Wextra -Werror -O2 -pthread examples/bad/abba_deadlock_pthread.c -o /tmp/d2.exe && /tmp/d2.exe
gcc -Wall -Wextra -Werror -O2 -pthread examples/good/nested_lock_ordering.c -o /tmp/d3.exe && /tmp/d3.exe
gcc -Wall -Wextra -Werror -O2 examples/bad/sleep_under_lock.c -o /tmp/d4.exe && /tmp/d4.exe
python examples/good/lockdep_cycle_detect.py
python examples/bad/lockdep_cycle_miss.py
```

Target kernel (RESEARCHED; build + boot required):

```
scripts/config -e PROVE_LOCKING -e DEBUG_ATOMIC_SLEEP -e LOCK_STAT
make -j$(nproc)
qemu-system-x86_64 -kernel arch/x86/boot/bzImage -append "console=ttyS0"
# exercise the new locking path; read dmesg for lockdep splats
```

## Where the knowledge comes from

- `kernel-lockdep-docs` — lockdep-design: lock classes, state rules, AB-BA,
  irq-safe/unsafe, nested locking, annotations, closure proof
- `kernel-source` — locking primitives and sleep classification
- `linux-rcu` — RCU read-side sections are non-sleep contexts
- `ldd3` — ch.5 spinlocks and atomic context
- `posix-threads` — pthread model used by the host fixtures
- `concurrency-deadlock-and-lock-ordering` — user-space twin of this skill

## Related skills

- `concurrency-deadlock-and-lock-ordering` (conflict) — user-space model;
  the kernel has extra context classes (IRQ, preempt, RCU)
- `kernel-rcu-memory-barriers` (recommend) — non-sleep contexts and RCU
  rules
- `kernel-atomic-context` (recommend) — what is forbidden in atomic
  contexts
- `data-race-kernel-detection` (recommend) — the other half of concurrency
  review
- `invariant-identification` (recommend) — state the lock-order invariant
  as the property lockdep checks

## Evaluation

- Synthetic: `bad/abba_deadlock_pthread.c` must be diagnosed as AB-BA and
  fixed by ordering; `bad/sleep_under_lock.c` must be diagnosed as
  sleep-in-atomic-context and fixed by pre-allocation; a fake-`_nested()`
  suppression must be rejected.
- False-positive: `good/lock_order_pthread.c` (consistent ordering),
  `good/nested_lock_ordering.c` (true subclass hierarchy), and a
  `spin_lock_irq`-correct pattern must NOT be flagged.
- Historical: real Linux lockdep-found deadlocks (e.g. the classic
  scsi/block or fs inversion classes; the rwsem-vs-mutex class) — the AB-BA
  shape is reproduced by the pthread fixture and the cycle detector.
- Adversarial: the AB-BA fixture is timing-scheduled to deadlock
  deterministically; an agent that "proves it safe" because one run
  completed reproduces the failure. A lockdep model that misses the cycle
  (`bad/lockdep_cycle_miss.py`) must be caught.
- Commands recorded on this host: `evals/README.md`.
