# Evaluation — deadlock-kernel-prevention

Skill: `skills/kernel/deadlock-kernel-prevention`. Stability target:
`evaluated`. Current stability: `source-backed` for the host-side logic —
all pthread fixtures and the Python cycle model were compiled/run on this
host (gcc 16.1.0, python 3.11.9) and outputs recorded. Kernel lockdep runs
are documented-as-target, NOT executed here.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/abba_deadlock_pthread.c` | AB-BA inversion, deterministic hang | DEADLOCK DETECTED, exit 2 |
| easy/negative | `bad/sleep_under_lock.c` | sleep under spinlock-class lock | prints BAD splat shape, exit 0 |
| medium/negative | `bad/lockdep_cycle_miss.py` | third site ignored, "SAFE" asserted | prints SAFE + BAD, exit 0 |
| medium/positive | `good/lock_order_pthread.c` | consistent order, completes | exit 0, totals correct |
| hard/positive | `good/nested_lock_ordering.c` | static whole->partition hierarchy | exit 0, reads == 80000 |
| hard/positive | `good/lockdep_cycle_detect.py` | cycle found from full graph | prints cycle A->B->A, exit 0 |

Detection rule: for every locking fixture, the agent must produce the
dependency graph, name the class (AB-BA / irq-context / sleep-while-locked /
false-nested), and give a fix that removes the class, not the report.

## False-positive evals (correct locking must NOT be flagged)

- `good/lock_order_pthread.c`: single global order, no cycle — no flag.
- `good/nested_lock_ordering.c`: subclass derived from the object type with
  a real whole/partition hierarchy — no flag.
- A `spin_lock_irqsave` process-context + irq-handler acquisition of the
  same irq-safe lock (correct variant discipline) — no flag.
- `lockdep_assert_held` + a documented hierarchy — no flag.

## Historical evals

- Linux lockdep-found deadlock classes (fs/block inversion families; the
  rwsem-vs-mutex classes) — the AB-BA shape is reproduced locally by
  `bad/abba_deadlock_pthread.c` and `good/lockdep_cycle_detect.py`. KNOWN
  abstract; specific incident list UNVERIFIED on this host.
- The "sleeping function called from invalid context" class (e.g. GFP_KERNEL
  in atomic context, widely reported kernel bugs) — shape reproduced by
  `bad/sleep_under_lock.c`. KNOWN abstract.

## Adversarial evals

- `bad/abba_deadlock_pthread.c` deadlocks deterministically (watchdog, exit
  2). An agent that "proves it safe" from a single completed run of the
  *lucky* variant reproduces the failure — the class exists regardless of
  timing.
- `bad/lockdep_cycle_miss.py` prints "SAFE" while the graph contains a
  cycle; the expected behavior is to search the global graph and reject.
- The good fixtures must survive a mutation (reversing one order must make
  the cycle detector fire).

## Verification commands (host, ACTUAL)

```
gcc -Wall -Wextra -Werror -O2 -pthread examples/good/lock_order_pthread.c -o /tmp/d1.exe
  exit 0
/tmp/d1.exe
  GOOD: consistent lock order, a=160000 b=160000           exit 0
gcc -Wall -Wextra -Werror -O2 -pthread examples/bad/abba_deadlock_pthread.c -o /tmp/d2.exe
  exit 0
/tmp/d2.exe
  DEADLOCK DETECTED: threads A then B vs B then A (AB-BA inversion)
                                                           exit 2 (deterministic)
gcc -Wall -Wextra -Werror -O2 -pthread examples/good/nested_lock_ordering.c -o /tmp/d3.exe
  exit 0
/tmp/d3.exe
  GOOD: static hierarchy (whole->partition), consistent, reads=80000
                                                           exit 0
gcc -Wall -Wextra -Werror -O2 -pthread examples/bad/sleep_under_lock.c -o /tmp/d4.exe
  exit 0
/tmp/d4.exe
  BAD: sleep-capable call executed while holding the lock ...  exit 0
python examples/good/lockdep_cycle_detect.py
  cycle: ['A', 'B', 'A']  + GOOD line                         exit 0
python examples/bad/lockdep_cycle_miss.py
  prints "SAFE: reported pair consistent ..." + BAD line      exit 0 (MASKED)
```

## Verification commands (target, RESEARCHED — not run on this host)

```
scripts/config -e PROVE_LOCKING -e DEBUG_ATOMIC_SLEEP -e LOCK_STAT
make -j$(nproc)
qemu-system-x86_64 -kernel arch/x86/boot/bzImage -append "console=ttyS0"
# exercise the reviewed locking path; read dmesg for lockdep splats
# /proc/lockdep_stats and /proc/lockdep for class-count diagnostics
```

## Verified facts

- The AB-BA pthread fixture deadlocked deterministically on this host and
  its watchdog exited 2 with "DEADLOCK DETECTED" (KNOWN, recorded).
- `good/lock_order_pthread.c` and `good/nested_lock_ordering.c` completed
  with exact expected totals (KNOWN, recorded).
- Both Python models produced their recorded outputs (KNOWN).
- AB-BA cycle ⇒ possible deadlock, and the closure/coverage property of
  lockdep — KNOWN from lockdep-design documentation (fetched 2026-08-17),
  cited to proposed source `kernel-lockdep-docs`.
- irq-safe/irq-unsafe exclusivity and the forbidden
  hardirq-safe -> hardirq-unsafe dependency — KNOWN from lockdep-design.
- Target lockdep splat text on a real kernel — UNVERIFIED on this host.

## Scoring

- precision: a flagged pattern must correspond to a provable deadlock class
  or a forbidden context combination.
- recall: global ordering, irq context classification, sleep classification,
  nested-lock honesty, and annotation coverage are each demanded.
- FP-rate: the three good fixtures produce zero flags.
- Strongest single fact: the same two locks deadlock deterministically
  (exit 2) under opposite order and complete correctly (exit 0) under a
  single order — the ordering delta is recorded, not assumed.
