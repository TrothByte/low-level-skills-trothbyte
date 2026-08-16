# Kernel Deadlock Prevention: IRQ Contexts and lockdep Discipline

## 1. irq-safe vs irq-unsafe: a lock's context classification is a hard property

- **RULE**: a lock that is ever acquired in an IRQ context is irq-safe; a
  lock ever acquired with IRQs enabled is irq-unsafe. A lock cannot be
  both: if an irq-unsafe lock is held when an IRQ takes an irq-safe lock,
  the IRQ handler self-deadlocks. Hence irq-safe locks must be acquired
  with IRQs disabled in lower contexts (`spin_lock_irq`/`spin_lock_irqsave`),
  and `hardirq-safe -> hardirq-unsafe` dependencies are forbidden. KNOWN
  (lockdep-design, "Single-lock state rules", "Multi-lock dependency
  rules").
- **WHY AI GETS IT WRONG**: agents add `spin_lock` in a bottom-half handler
  and plain `spin_lock` in process context, missing the interrupt can
  preempt the process-context holder.
- **CORRECT REASONING**: for every lock, write the context matrix (hardirq /
  softirq / process × enabled/disabled) and force consistent variants
  (`_irq`, `_bh`, `_irqsave`) per context.
- **EXAMPLE** (bad): process context uses `spin_lock` on a lock the IRQ
  handler also takes — interrupt self-deadlock.
- **COUNTEREXAMPLE** (good): process context uses `spin_lock_irqsave`; the
  lock is correctly classified irq-safe.
- **VERIFICATION**: host fixtures model the classification; target lockdep
  prints the "{+...-}" usage state in splats (documented).
- **SOURCE**: kernel-lockdep-docs (State; lock usage bits) [proposed].

## 2. lockdep_assert_held* converts assumptions into gates

- **RULE**: `lockdep_assert_held(&lock)` and the `lockdep_*pin_lock` family
  generate a WARN when the asserted lock is not held at that point. They
  turn "I assume the caller holds the lock" into a runtime-checked
  requirement. KNOWN (lockdep-design, "Annotations").
- **WHY AI GETS IT WRONG**: annotations are treated as comments; agents
  neither add them nor read them as executable requirements.
- **CORRECT REASONING**: whenever a function's correctness depends on a held
  lock, assert it; the annotation is the executable statement of the
  precondition (see invariant-identification).
- **EXAMPLE** (bad): a helper mutating shared state without asserting the
  lock; a future caller drops the lock and nothing warns.
- **COUNTEREXAMPLE** (good): the helper starts with
  `lockdep_assert_held(&q->lock)`; the bad caller triggers the WARN.
- **VERIFICATION**: fixtures model the gate; target builds with
  CONFIG_PROVE_LOCKING exercise the path (documented).
- **SOURCE**: kernel-lockdep-docs (Annotations) [proposed].

## 3. lockdep proves deadlock *classes* from partial executions

- **RULE**: once lockdep has seen every simple lock chain at least once, it
  proves (its own correctness assumed) that no combination of timings,
  tasks, or CPUs can produce a lock-related deadlock. That is both stronger
  than testing and dependent on coverage: paths never executed contribute
  no chains. KNOWN (lockdep-design, "Proof of 100% correctness",
  "Performance").
- **WHY AI GETS IT WRONG**: "lockdep clean" is treated as absolute even when
  the new code path was never exercised; or conversely, one splat is treated
  as proof the code always deadlocks (it proves a possible class, which is
  enough to demand a fix).
- **CORRECT REASONING**: interpret the report correctly: a splat = a proven
  deadlock class (fix required); silence = no chain seen (still needs
  coverage + reasoning about the global order).
- **EXAMPLE** (bad): adding lockdep_off() to silence a real cycle.
- **COUNTEREXAMPLE** (good): the cycle is removed by ordering; lockdep
  silence is then a positive signal on exercised paths.
- **VERIFICATION**: the cycle model's output distinguishes "no cycle in
  graph" from "cycle not searched".
- **SOURCE**: kernel-lockdep-docs (Proof of closure; Performance) [proposed].

## 4. Lock classes: init matters, and lock-class leaks are detection failures

- **RULE**: lockdep operates on lock *classes*; statically initialized locks
  in arrays each become their own class unless explicitly initialized at
  runtime (`spin_lock_init`), which can exhaust MAX_LOCKDEP_KEYS and
  silently disable the validator for new chains. Repeated module
  load/unload leaks classes. KNOWN (lockdep-design, "Troubleshooting").
- **WHY AI GETS IT WRONG**: agents don't init per-instance locks and later
  attribute the "lockdep ran out" warning to a noise bug, losing detection
  exactly when the graph grows.
- **CORRECT REASONING**: initialize locks in the object's init path; watch
  `/proc/lockdep_stats` lock-class count for growth.
- **EXAMPLE** (bad): a 8192-bucket hash with static lock initializers
  consumes thousands of classes.
- **COUNTEREXAMPLE** (good): the loop calls `spin_lock_init` per bucket —
  one class.
- **VERIFICATION**: `grep "lock-classes" /proc/lockdep_stats` documented as
  the target check.
- **SOURCE**: kernel-lockdep-docs (Lock-class; Troubleshooting) [proposed].
