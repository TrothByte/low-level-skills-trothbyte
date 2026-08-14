---
name: kernel-atomic-context
description: Use when writing, reviewing, or debugging Linux kernel code that runs in atomic context: interrupt handlers, bottom halves, spinlock-held or preemption-disabled regions. Covers what is forbidden there (sleeping kmalloc/mutex/schedule), GFP_ATOMIC, irqsave/bh lock variants, deferring to process context, and verifying with lockdep.
---

# Linux Kernel Atomic Context Discipline

## When to use

- Writing or reviewing code that can run in atomic context: interrupt
  handlers (hardirq), bottom halves (softirq/tasklet), spinlock-held or
  preemption-disabled regions, RCU read-side sections.
- Choosing an allocation flag, lock type, or deferral mechanism when the
  calling context is not guaranteed to be process context.
- Debugging "BUG: scheduling while atomic" or "sleeping function called
  from invalid context" splats and lockdep reports.
- Reviewing patches that add a `kmalloc`, `mutex_lock`, `copy_to_user`, or
  `schedule()` inside a locking region or interrupt handler.

## When not to use

- User-space threads/pthreads and host mutexes — different model
  (`concurrency-deadlock-and-lock-ordering`).
- FreeRTOS/Zephyr ISR rules — different API and context model
  (`rtos-concurrency-and-isr`).
- Barrier/ordering-only questions with no sleeping-legality issue
  (`kernel-rcu-memory-barriers`).
- Pure memory-ordering reasoning on the host (`memory-ordering-reasoning`).

## What the agent often gets wrong

- "kmalloc(GFP_KERNEL) inside a spinlock is fine unless it fails."
  GFP_KERNEL may sleep; a spinlock disables preemption, so the sleep is a
  bug regardless of whether the allocation succeeds.
- "mutex_lock is OK in an interrupt handler because the mutex will be
  free." `mutex_lock` may sleep even on the happy path; interrupt context
  cannot sleep at all.
- "schedule() can be called anywhere in the kernel." With preemption
  disabled or from interrupt context it deadlocks or splats.
- "in_interrupt()/in_atomic() reliably tell me if I can sleep." They are
  heuristics over `preempt_count`; lockdep + CONFIG_DEBUG_ATOMIC_SLEEP are
  the authoritative checkers.
- "A tasklet can sleep because a workqueue can sleep." Tasklets run in
  softirq context (atomic); only workqueue/kthread callbacks run in process
  context.
- "spin_lock_irq and spin_lock are interchangeable." For a lock shared
  with an interrupt context, plain `spin_lock` deadlocks the moment the
  interrupt fires on the same CPU.
- "GFP_ATOMIC can never fail, so no NULL check is needed." It can fail
  under memory pressure; the result must be checked.

## How to reason correctly

1. Classify the context first: process context (may sleep), softirq/tasklet
   (atomic), hardirq (atomic), RCU read-side (no sleeping; atomic on
   non-PREEMPT_RCU builds).
2. Trace every function reachable from an atomic region; any call that can
   sleep (GFP_KERNEL kmalloc, mutex, schedule, uaccess that may fault,
   wait_event, synchronize_rcu) is a bug.
3. In atomic context allocate with GFP_ATOMIC and check the result; lock
   with spinlocks, not mutexes.
4. Pick the lock variant from the context set the lock is shared across:
   process-only -> `spin_lock`; process+hardirq -> `spin_lock_irqsave` /
   `spin_unlock_irqrestore`; process+softirq -> `spin_lock_bh` /
   `spin_unlock_bh`.
5. Defer sleeping work: hardirq -> (tasklet/softirq, still atomic) ->
   workqueue/kthread, where sleeping is legal. Copy only small state in the
   handler.
6. Never rely on `in_interrupt()`/`in_atomic()` to decide legality; verify
   with lockdep and CONFIG_DEBUG_ATOMIC_SLEEP builds.

## What to verify

- No sleeping call (GFP_KERNEL kmalloc, mutex_lock, copy_*_user, schedule,
  wait_event, synchronize_rcu) is reachable from any atomic region.
- kmalloc in atomic regions uses GFP_ATOMIC and its result is checked.
- Locks shared with interrupt context use irqsave/irqrestore (or bh
  variants for softirq sharing); no plain spin_lock for interrupt-shared
  locks.
- Work deferred from atomic context runs in a workqueue/kthread (sleeping
  legal), not in a tasklet, unless the deferred work is itself atomic-safe.
- lockdep + CONFIG_DEBUG_ATOMIC_SLEEP builds boot clean with no splat.

## How to verify

Host-compilable logic checks (self-contained stubs, no kernel headers):

```
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_gfp_kernel_in_spinlock.c -o /tmp/bad1 && /tmp/bad1
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_mutex_in_interrupt.c -o /tmp/bad2 && /tmp/bad2
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_schedule_in_atomic.c -o /tmp/bad3 && /tmp/bad3
gcc -Wall -Wextra -Werror -O2 examples/good/good_gfp_atomic.c -o /tmp/good1 && /tmp/good1
gcc -Wall -Wextra -Werror -O2 examples/good/good_workqueue_deferral.c -o /tmp/good2 && /tmp/good2
gcc -Wall -Wextra -Werror -O2 examples/good/good_spin_lock_irqsave.c -o /tmp/good3 && /tmp/good3
```

Target kernel checks — documented, NOT run on this host:

```
make defconfig
scripts/config -e PROVE_LOCKING -e DEBUG_ATOMIC_SLEEP -e KASAN
make -j$(nproc)
qemu-system-x86_64 -kernel arch/x86/boot/bzImage \
  -append "console=ttyS0 nokaslr" -nographic
# boot, exercise the driver path, then check dmesg for:
#   "BUG: sleeping function called from invalid context"
#   "BUG: scheduling while atomic"
```

## Where the knowledge comes from

- `ldd3` — ch.5 spinlocks/atomic context, ch.7 tasklets/workqueues,
  ch.8 kmalloc GFP flags, ch.10 interrupt handling
- `linux-memory-barriers` — locking, interrupt-disable, and ordering rules
- `kernel-coding-style` — kernel conventions and lock usage
- `linux-rcu` — RCU read-side contexts and sleeping restrictions

## Related skills

- `kernel-rcu-memory-barriers` — RCU/barrier rules including no-sleep in
  atomic context (overlap; load both for RCU paths)
- `kernel-uaccess-safety` — copy_to_user/copy_from_user legality in atomic
  context
- `concurrency-deadlock-and-lock-ordering` — lock ordering and deadlock
  analysis
- `rtos-concurrency-and-isr` — RTOS analog: ISR-safe APIs and deferral
- `c-signal-handler-safety` — async-signal-safe discipline for host signals
- `embedded-volatile-and-memory-ordering` — ISR/MMIO ordering on bare metal

## Evaluation

Synthetic: GFP_KERNEL under spinlock, mutex_lock in interrupt, schedule()
in atomic context, tasklet performing sleeping work — each must be detected
and fixed. Adversarial: code that "passes" a single-CPU smoke test because
the bad path is never exercised under load or interrupt timing. False-
positive: GFP_ATOMIC with NULL check, irqsave/irqrestore around an
interrupt-shared lock, workqueue deferral with GFP_KERNEL inside the work
function — must NOT be flagged.
