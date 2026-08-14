# Linux Kernel Atomic Context Rules

Source-backed rule set for what is legal in atomic context. Each entry:
RULE -> WHY AI GETS IT WRONG -> CORRECT REASONING -> EXAMPLE -> COUNTEREXAMPLE
-> VERIFICATION -> SOURCE. Confidence markers: KNOWN (documented kernel
contract), INFERRED (derived from it).

## 1. What "atomic context" means

- **RULE**: Atomic context is any region where the kernel cannot sleep:
  interrupt handlers (hardirq), bottom halves (softirq/tasklet), and any
  region with preemption disabled — spinlock held, `preempt_disable()`,
  local irqs disabled, or inside an RCU read-side section. It does NOT mean
  "atomic operations only"; it means the scheduler cannot be reached.
- **WHY AI GETS IT WRONG**: equates "atomic context" with just interrupt
  handlers, and forgets spinlock-held and preemption-disabled regions; or
  thinks a sleeping function is fine "as long as it does not actually
  contend".
- **CORRECT REASONING**: sleeping means giving up the CPU, which requires
  the scheduler, which requires preemption. When preemption is disabled
  (spinlock held, irq off, interrupt entry) or the task is not runnable in
  the normal sense, a sleep cannot proceed: the kernel either deadlocks or
  emits "BUG: scheduling while atomic" / "sleeping function called from
  invalid context". Legality is a property of the calling context, not of
  the called function's happy path (KNOWN).
- **EXAMPLE** (bad):
  ```c
  spin_lock(&d->lock);
  kmalloc(size, GFP_KERNEL);   /* may sleep inside the lock */
  spin_unlock(&d->lock);
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  spin_lock(&d->lock);
  kmalloc(size, GFP_ATOMIC);   /* never sleeps */
  spin_unlock(&d->lock);
  ```
- **VERIFICATION**: lockdep (`CONFIG_PROVE_LOCKING`) +
  `CONFIG_DEBUG_ATOMIC_SLEEP` build; the splat is in dmesg on the first
  reachable sleep. Host stub: the emulated `kmalloc(GFP_KERNEL)` under a
  held spinlock records `g_sleep_in_atomic`.
- **SOURCE**: ldd3 (ch. 5 spinlocks/atomic context, ch. 10 interrupts);
  linux-memory-barriers (locking and interrupt-disable ordering).

## 2. Sleeping calls are forbidden in atomic context

- **RULE**: `kmalloc(GFP_KERNEL)`, `mutex_lock`, `schedule()`, `wait_event`,
  `copy_to_user`/`copy_from_user` (page fault may sleep), `synchronize_rcu()`
  are forbidden in atomic context. Anything that can block, wait, or fault
  is a bug there.
- **WHY AI GETS IT WRONG**: treats each call in isolation ("this mutex is
  uncontended") instead of checking the reachable set of calls from the
  atomic region.
- **CORRECT REASONING**: a single reachable sleeping call is enough to make
  the whole path wrong, even if it never triggers in tests. The failure
  modes are a hard hang (deadlock) or an oops/splat, both production-critical
  (KNOWN). Verify by tracing the full call tree from the atomic region.
- **EXAMPLE** (bad): `mutex_lock(&cfg->lock);` inside a hardirq handler.
- **COUNTEREXAMPLE** (good): a spinlock (non-sleeping) or a deferred
  workqueue job that takes the mutex in process context.
- **VERIFICATION**: code review of the call graph; lockdep reports the exact
  sleep path with a stack trace.
- **SOURCE**: ldd3 (ch. 5, ch. 10); kernel-coding-style.

## 3. `kmalloc`: `GFP_KERNEL` -> `GFP_ATOMIC` in atomic context

- **RULE**: `GFP_KERNEL` may sleep to satisfy the allocation; it is only
  legal in process context. In atomic context use `GFP_ATOMIC`, which never
  sleeps, and check the result (it can fail).
- **WHY AI GETS IT WRONG**: "GFP_KERNEL is the default, so it must be the
  safe one"; or "the allocation succeeds anyway in my test so it is fine".
- **CORRECT REASONING**: `GFP_KERNEL` blocks on memory reclaim
  (`__alloc_pages` can wait for the page allocator), which is a sleep. In
  atomic context that sleep is illegal; `GFP_ATOMIC` instead takes from
  reserves and never blocks (KNOWN, ldd3 ch. 8). `GFP_ATOMIC` failing under
  pressure is real, so a NULL check is mandatory.
- **EXAMPLE** (bad):
  ```c
  spin_lock(&d->lock);
  d->buf = kmalloc(size, GFP_KERNEL);
  spin_unlock(&d->lock);
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  spin_lock(&d->lock);
  d->buf = kmalloc(size, GFP_ATOMIC);
  if (!d->buf)
      ret = -ENOMEM;         /* handle failure, never sleep */
  spin_unlock(&d->lock);
  ```
- **VERIFICATION**: host stub: GFP_KERNEL under a held spinlock records the
  sleep; GFP_ATOMIC does not and returns a buffer. Target:
  `CONFIG_DEBUG_ATOMIC_SLEEP` splat on the first run.
- **SOURCE**: ldd3 (ch. 8, kmalloc and GFP flags); kernel-coding-style.

## 4. `mutex_lock` -> spinlock / `spin_lock_irqsave` in atomic context

- **RULE**: `mutex_lock`/`mutex_unlock` may sleep and are forbidden in
  atomic context. Protect the shared data with a spinlock instead; choose
  `spin_lock_irqsave`/`spin_unlock_irqrestore` when the same lock is taken
  from interrupt context too.
- **WHY AI GETS IT WRONG**: mutexes feel like "the obvious locking API";
  agents swap them into interrupt handlers without checking the sleep
  semantics.
- **CORRECT REASONING**: a mutex has an owner and blocks (sleeps) when
  contended; that is precisely what atomic context cannot do. A spinlock
  disables preemption and spins briefly instead of sleeping — legal for the
  short critical sections that belong in atomic context (KNOWN, ldd3 ch. 5).
- **EXAMPLE** (bad):
  ```c
  mutex_lock(&cfg->lock);        /* in an IRQ handler */
  cfg->live = 1;
  mutex_unlock(&cfg->lock);
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  unsigned long flags;
  spin_lock_irqsave(&cfg->lock, flags);
  cfg->live = 1;
  spin_unlock_irqrestore(&cfg->lock, flags);
  ```
- **VERIFICATION**: host stub: `mutex_lock_emu` in simulated interrupt
  context records `g_sleep_in_atomic`; the irqsave path does not. Target:
  lockdep reports "sleeping function called from invalid context".
- **SOURCE**: ldd3 (ch. 5 mutex vs spinlock); linux-memory-barriers
  (locking and interrupt disabling).

## 5. `schedule()` and blocking waits are illegal; why they deadlock

- **RULE**: never call `schedule()` (or a blocking wait such as
  `wait_event`) while holding a spinlock, with preemption disabled, or from
  interrupt context. The scheduler itself runs with runqueue spinlocks held
  and requires preemption to switch tasks.
- **WHY AI GETS IT WRONG**: "schedule() is how you yield; I just want to
  give up the CPU." The scheduler cannot run when preemption is disabled,
  so the call cannot complete.
- **CORRECT REASONING**: `__schedule()` takes `rq->lock` and manipulates
  runqueues; calling it while a spinlock is already held on the same CPU
  means acquiring a spinlock the current code already owns (self-deadlock)
  or, with `CONFIG_DEBUG_ATOMIC_SLEEP`/preempt-count checks, an immediate
  "BUG: scheduling while atomic" splat (KNOWN). A task that must wait inside
  a spinlock must instead defer the wait to process context.
- **EXAMPLE** (bad):
  ```c
  spin_lock(&r->lock);
  while (r->tail > 0) {
      r->tail--;
      schedule();            /* deadlock / scheduling-while-atomic */
  }
  spin_unlock(&r->lock);
  ```
- **COUNTEREXAMPLE** (good): collect the work in the spinlock, release it,
  then do the waiting/processing in process context (or a workqueue).
- **VERIFICATION**: host stub: `schedule_emu()` with `g_atomic_depth > 0`
  records `g_schedule_in_atomic`. Target: `BUG: scheduling while atomic` in
  dmesg; lockdep splat.
- **SOURCE**: ldd3 (ch. 5 atomic context); linux-memory-barriers (locking).

## 6. Bottom halves: softirq/tasklet are atomic; workqueue is not

- **RULE**: softirq and tasklet handlers run in softirq (bottom-half)
  context — atomic, cannot sleep. Workqueue callbacks run in process context
  (kworker thread) and may sleep. The deferral chain is hardirq ->
  softirq/tasklet (short, atomic) -> workqueue/kthread (sleeping allowed).
- **WHY AI GETS IT WRONG**: lumps "bottom half" together and assumes a
  tasklet may sleep "because workqueues can", or assumes a workqueue
  callback is atomic like a tasklet.
- **CORRECT REASONING**: tasklets are built on softirqs, which run with
  bottom halves disabled and preemption off — sleeping there is the same
  bug as in a spinlock (KNOWN). Workqueues are executed by kernel threads,
  so a `GFP_KERNEL` allocation or `mutex_lock` inside a work function is
  legal. Choose workqueue for sleeping work; keep tasklets atomic-only.
- **EXAMPLE** (bad): a tasklet that calls `kmalloc(size, GFP_KERNEL)`.
- **COUNTEREXAMPLE** (good): the tasklet records the request and queues a
  work item; the work function allocates with GFP_KERNEL.
- **VERIFICATION**: host stub: running `kmalloc(GFP_KERNEL)` while
  `g_bh_disabled > 0` records `g_sleep_in_atomic`. Target:
  `CONFIG_DEBUG_ATOMIC_SLEEP` splat inside the tasklet path.
- **SOURCE**: ldd3 (ch. 7 tasklets and workqueues); linux-memory-barriers.

## 7. `spin_lock` vs `spin_lock_irqsave` vs `spin_lock_bh`

- **RULE**: choose the variant from the set of contexts the lock is shared
  across: process context only -> `spin_lock`; process + hardirq ->
  `spin_lock_irqsave`/`spin_unlock_irqrestore`; process + softirq ->
  `spin_lock_bh`/`spin_unlock_bh`.
- **WHY AI GETS IT WRONG**: uses plain `spin_lock` everywhere because it
  compiles and "usually works", ignoring that an interrupt on the same CPU
  can then deadlock on the same lock.
- **CORRECT REASONING**: a plain `spin_lock` disables preemption but not
  local interrupts; if the lock is taken both by process code and by an
  interrupt handler on the same CPU, the interrupt can fire while the lock
  is held and spin on it forever (self-deadlock, KNOWN ldd3 ch. 5).
  `_irqsave` disables local irqs and saves the old state, so the interrupt
  path cannot run concurrently; `_bh` disables softirqs instead. `_irqsave`
  is preferred over `spin_lock_irq` because it restores the previous state
  instead of assuming irqs were enabled on entry.
- **EXAMPLE** (bad): a lock used by a hardirq handler and by process code,
  locked with plain `spin_lock` in both places.
- **COUNTEREXAMPLE** (good): `spin_lock_irqsave(&l, flags)` /
  `spin_unlock_irqrestore(&l, flags)` in process context; the handler uses
  the same pair.
- **VERIFICATION**: host stub: irqsave captures the irq state in `flags`
  and restores it; a plain spinlock while `g_hardirq` is set would leave
  irqs "enabled" and is flagged by review. Target: lockdep "possible
  deadlock" report for the plain-variant pair.
- **SOURCE**: ldd3 (ch. 5 spinlock variants); linux-memory-barriers
  (interrupt disabling as an ordering guarantee).

## 8. `in_interrupt()` / `in_atomic()` are heuristic, not authoritative

- **RULE**: `in_interrupt()`, `in_softirq()`, `in_atomic()` decode bits of
  `preempt_count` and are unreliable for deciding whether sleeping is legal.
  Do not gate a sleeping call on them. Use lockdep and
  `CONFIG_DEBUG_ATOMIC_SLEEP` for the real answer.
- **WHY AI GETS IT WRONG**: writes `if (!in_interrupt()) mutex_lock(...);`
  and believes the guard makes the sleep safe.
- **CORRECT REASONING**: these macros are context heuristics (KNOWN: they
  inspect the hardirq/softirq/preempt-disable counters), and they are
  known to be unreliable in edge cases (INFERRED: preempt-count
  configuration differences such as `CONFIG_PREEMPT_RCU` or scheduler
  internals change what the bits mean). The correct answer to "can I sleep
  here?" is the context of the caller, determined statically and verified
  by lockdep, not a runtime counter probe.
- **EXAMPLE** (bad):
  ```c
  if (!in_interrupt())
      mutex_lock(&m);   /* IRQ state is not a permission to sleep */
  ```
- **COUNTEREXAMPLE** (good): establish the context from the call path
  (process-context callback, work function, or explicitly deferred job)
  and use lockdep-verified primitives.
- **VERIFICATION**: review the call path, not the macro result; a
  lockdep/`CONFIG_DEBUG_ATOMIC_SLEEP` build flags the reachable sleep.
- **SOURCE**: kernel-coding-style; ldd3 (ch. 10 in_interrupt usage and
  caveats).

## 9. Deferring to process context

- **RULE**: when atomic context needs sleeping work — allocation, mutex
  taking, uaccess, waiting — copy only the minimal state in the atomic
  region and defer the work to a workqueue, kthread, or other process-
  context path where sleeping is legal.
- **WHY AI GETS IT WRONG**: does the sleeping work inline "because it is
  faster", or defers too little (e.g., holds a spinlock across the deferral).
- **CORRECT REASONING**: the handler must be short and non-blocking;
  anything that can sleep must run elsewhere. `copy_to_user`/`copy_from_user`
  from atomic context is also forbidden because the fault handler may sleep
  (KNOWN). Deferral is the standard design: record -> queue -> process.
- **EXAMPLE** (bad): an interrupt handler that calls
  `copy_to_user(...)` to report the event directly.
- **COUNTEREXAMPLE** (good): the handler queues a work item carrying a
  snapshot; the work function runs in process context and performs the
  uaccess and any sleeping operations.
- **VERIFICATION**: host stub: only `queue_work_emu` is called under
  `g_hardirq`; the uaccess/allocation runs inside `flush_work_emu` (process
  context) with no `g_sleep_in_atomic` set. Target: lockdep clean under
  load.
- **SOURCE**: ldd3 (ch. 7 workqueues, ch. 10 deferring interrupt work);
  linux-memory-barriers.

## 10. irq-safe allocation still fails — check the result

- **RULE**: `GFP_ATOMIC` does not sleep, but it is not guaranteed to
  succeed: under memory pressure it can return NULL. Every allocation in
  atomic context must have a NULL check and a defined failure path.
- **WHY AI GETS IT WRONG**: "GFP_ATOMIC is special, so it never fails" —
  then dereferences the result.
- **CORRECT REASONING**: `GFP_ATOMIC` bypasses sleeping reclaim and draws
  on reserves, so it succeeds more often, but it is still a fallible
  allocation (KNOWN). The failure path must not sleep either (no retry with
  GFP_KERNEL inside the atomic region).
- **EXAMPLE** (bad):
  ```c
  d->buf = kmalloc(size, GFP_ATOMIC);
  d->buf[0] = 1;          /* NULL deref when allocation fails */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  d->buf = kmalloc(size, GFP_ATOMIC);
  if (!d->buf)
      return -ENOMEM;
  d->buf[0] = 1;
  ```
- **VERIFICATION**: host stub: force `kmalloc_emu` to fail by passing
  `GFP_ATOMIC` under an artificial pressure flag (INFERRED model) and assert
  the NULL path is taken; code review for missing checks.
- **SOURCE**: ldd3 (ch. 8, kmalloc return value); kernel-coding-style.

## 11. RCU read-side sections cannot sleep

- **RULE**: RCU read-side critical sections (`rcu_read_lock()`/
  `rcu_read_unlock()`) may not contain sleeping calls: no blocking, no
  allocation with `GFP_KERNEL`, no `synchronize_rcu()`. On non-PREEMPT_RCU
  builds the read-side section is also an atomic context (preemption
  disabled); on `CONFIG_PREEMPT_RCU` it may preempt but still cannot block.
- **WHY AI GETS IT WRONG**: thinks "read lock is a shared lock like a
  semaphore, so sleeping is fine if I do not write".
- **CORRECT REASONING**: `rcu_read_lock()` disables preemption (or
  switching) for the section; a sleeping call inside it is the same
  sleep-in-atomic-context bug and additionally extends the grace period.
  `synchronize_rcu()` itself sleeps, so it can never be called from inside
  a read-side section or with a read lock held (KNOWN, linux-rcu).
- **EXAMPLE** (bad):
  ```c
  rcu_read_lock();
  p = rcu_dereference(g_ptr);
  if (kmalloc(size, GFP_KERNEL) == NULL)  /* sleeps in RCU read section */
      ...
  rcu_read_unlock();
  ```
- **COUNTEREXAMPLE** (good): allocate before `rcu_read_lock()` or after
  `rcu_read_unlock()`; keep the read-side section to pointer reads.
- **VERIFICATION**: `CONFIG_PROVE_RCU` + lockdep build; RCU lockdep emits a
  "suspicious RCU usage" report.
- **SOURCE**: linux-rcu (checklist: read-side critical sections may not
  sleep); linux-memory-barriers (RCU ordering).

## Quick detection table

| Pattern | Class | Check |
|---|---|---|
| `kmalloc(GFP_KERNEL)` under a lock/irq | sleep in atomic | use GFP_ATOMIC + NULL check |
| `mutex_lock` in IRQ/bottom half | sleep in atomic | spinlock / `_irqsave` / defer |
| `schedule()` in atomic context | deadlock | defer to process context |
| tasklet doing sleeping work | sleep in atomic | move to workqueue |
| plain `spin_lock` on an irq-shared lock | self-deadlock | `spin_lock_irqsave` |
| `copy_to_user` from atomic context | fault->sleep | defer to process context |
| `GFP_ATOMIC` result dereferenced | NULL deref | check before use |
| `in_interrupt()` gating a sleep | heuristic | lockdep / DEBUG_ATOMIC_SLEEP |
| sleep inside `rcu_read_lock()` | RCU usage | keep section non-blocking |
