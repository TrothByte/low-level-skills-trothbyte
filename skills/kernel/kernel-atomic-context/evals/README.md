# Evaluation — kernel-atomic-context

Skill: `skills/kernel/kernel-atomic-context`. Stability target: `evaluated`.

## Verified facts (host, this run)

- All examples compile clean with `gcc -Wall -Wextra -Werror -O2` (GCC 16.1,
  MSYS2) using self-contained stubs (`examples/stubs.h`) — no kernel headers
  required.
- Good examples run with assertions passing (exit 0).
- Bad examples compile and run; each reproduces its flaw deterministically
  and prints a "BUG reproduced" diagnostic (exit 0) without crashing.

| Example | Compile | Run | Observation |
|---|---|---|---|
| good/good_gfp_atomic.c | 0 | 0 | GFP_ATOMIC under spinlock: no sleep, result checked |
| good/good_workqueue_deferral.c | 0 | 0 | irq defers; GFP_KERNEL runs in workqueue process context |
| good/good_spin_lock_irqsave.c | 0 | 0 | irqsave/restore pair; counter correct, no sleep |
| bad/bad_gfp_kernel_in_spinlock.c | 0 | 0 | "BUG reproduced: kmalloc(GFP_KERNEL) in spinlock" |
| bad/bad_mutex_in_interrupt.c | 0 | 0 | "BUG reproduced: mutex_lock() in interrupt context" |
| bad/bad_schedule_in_atomic.c | 0 | 0 | "BUG reproduced: schedule() in atomic context" |

NOT verified on this host (documented targets, do NOT claim to have run):
kernel build, lockdep (`CONFIG_PROVE_LOCKING`), `CONFIG_DEBUG_ATOMIC_SLEEP`,
KASAN, QEMU boot, syzkaller runs.

## Synthetic evals

- easy/positive: `kmalloc(size, GFP_ATOMIC)` with a NULL check inside a
  spinlock must NOT be flagged.
- easy/negative: `kmalloc(size, GFP_KERNEL)` inside a spinlock must be
  flagged (rule 3: GFP flag choice).
- medium/negative: `mutex_lock` inside an interrupt handler must be flagged
  (rule 4: mutex vs spinlock).
- medium/negative: `schedule()` in atomic context must be flagged (rule 5:
  deadlock).
- hard/negative: a tasklet that performs sleeping work (GFP_KERNEL) must be
  flagged even though the code "compiles" (rule 6: bottom-half context).
- hard/negative: a lock shared with interrupt context protected by plain
  `spin_lock` must be flagged (rule 7: irqsave variant).

## Adversarial evals

- Code that "passes" a single-CPU QEMU smoke test because the interrupt
  path is never taken under load — agent must not declare it correct.
- A workqueue deferral that still allocates GFP_KERNEL inside the
  *tasklet* stage (only the last stage is process context).
- A guard like `if (!in_interrupt()) mutex_lock(...)` presented as "safe" —
  agent must reject the heuristic gate (rule 8).

## False-positive evals (correct code must not be flagged)

- `kmalloc(size, GFP_ATOMIC)` with a NULL check inside a spinlock — do NOT
  flag.
- `spin_lock_irqsave` / `spin_unlock_irqrestore` around a lock shared with
  interrupt context — do NOT flag.
- Workqueue deferral where the handler only queues and the work function
  uses GFP_KERNEL/mutex in process context — do NOT flag.
- `spin_lock_bh` for a lock shared with softirq context — do NOT flag.

## Verification commands

Host (self-contained stubs — recorded this run):

```
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_gfp_kernel_in_spinlock.c -o /tmp/bad_gfp && /tmp/bad_gfp
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_mutex_in_interrupt.c -o /tmp/bad_mutex && /tmp/bad_mutex
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_schedule_in_atomic.c -o /tmp/bad_sched && /tmp/bad_sched
gcc -Wall -Wextra -Werror -O2 examples/good/good_gfp_atomic.c -o /tmp/good_gfp && /tmp/good_gfp
gcc -Wall -Wextra -Werror -O2 examples/good/good_workqueue_deferral.c -o /tmp/good_wq && /tmp/good_wq
gcc -Wall -Wextra -Werror -O2 examples/good/good_spin_lock_irqsave.c -o /tmp/good_irqsave && /tmp/good_irqsave
```

Target (kernel) — documented only, NOT run here:

```
make defconfig
scripts/config -e PROVE_LOCKING -e DEBUG_ATOMIC_SLEEP -e KASAN
make -j$(nproc)
qemu-system-x86_64 -kernel arch/x86/boot/bzImage \
  -append "console=ttyS0 nokaslr" -nographic
# exercise the driver path; expect NO splats in dmesg:
#   "BUG: sleeping function called from invalid context"
#   "BUG: scheduling while atomic"
# checkpatch.pl --strict your.patch
```

`CONFIG_DEBUG_ATOMIC_SLEEP` (via `might_sleep`) reports sleeping functions
called in atomic context; lockdep reports lock-context misuse (e.g. plain
`spin_lock` on an irq-shared lock).

## Scoring

- precision: every flagged pattern maps to a real atomic-context rule.
- recall: each bad snippet is detected by the stub (recorded violation).
- FP-rate: good snippets produce zero flags.
