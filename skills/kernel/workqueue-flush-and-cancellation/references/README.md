# Linux Kernel Workqueue Flush & Cancellation Rules

Source-backed rule set for workqueue shutdown safety. Each entry:
RULE -> WHY AI GETS IT WRONG -> CORRECT REASONING -> EXAMPLE -> COUNTEREXAMPLE
-> VERIFICATION -> SOURCE. Confidence markers: KNOWN (documented contract),
INFERRED (derived), UNVERIFIED (never use in a stable skill). All normative
statements are grounded in the Linux workqueue documentation and
kernel/workqueue.c. Emulator functions (`*_emu`) refer to
`examples/stubs.h`.

## 1. Work item states and the single-queue guarantee

- **RULE**: a `work_struct` is in exactly one of IDLE / PENDING / RUNNING. It
  is on at most one queue at a time, and `queue_work` only moves IDLE to
  PENDING — it returns false if the item is already pending (already on a
  queue). A second `queue_work` does not create a second queue entry.
- **WHY AI GETS IT WRONG**: agents treat `queue_work` like a list-push and
  assume repeated calls enqueue repeated executions.
- **CORRECT REASONING**: the PENDING state is a single bit: once set, further
  queue attempts are no-ops returning false. To run the item again you must
  first let it execute (or cancel it). This is why `queue_work` in a loop
  cannot "pile up" instances of one item.
- **EXAMPLE** (bad):
  ```c
  queue_work_emu(wq, &work);
  queue_work_emu(wq, &work);   /* false: already PENDING, no second entry */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (!queue_work_emu(wq, &work))
      /* already queued or running: one instance only, nothing to do */
      return;
  ```
- **VERIFICATION**: harness asserts the second queue call returns false and
  the queue holds one instance.
- **SOURCE**: kernel-workqueue-docs (work item / API); kernel-source
  (kernel/workqueue.c `__queue_work` pending-bit test).

## 2. `queue_work` vs `queue_work_on` (CPU affinity)

- **RULE**: `queue_work(wq, work)` queues on the CPU the caller runs on (for
  a bound workqueue). `queue_work_on(cpu, wq, work)` queues the item on a
  specific CPU, which is how drivers express CPU locality or affinity
  requirements for a per-CPU workqueue.
- **WHY AI GETS IT WRONG**: agents pick `queue_work` for everything and never
  think about which CPU executes the item, or pick `queue_work_on` with an
  invalid/offline CPU.
- **CORRECT REASONING**: bound workqueue items run on the CPU they were
  queued on (or migrate if that CPU dies). Choose `queue_work_on` when the
  work must touch per-CPU state on a known CPU; use a valid, online CPU id.
  `queue_work_on` shares `queue_work`'s ordering guarantee: if it returns
  true, stores before the call are visible to the worker before the item
  executes.
- **EXAMPLE** (bad):
  ```c
  int cpu = smp_processor_id();       /* may be offline at teardown */
  queue_work_on_emu(cpu, wq, &work);
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int cpu = cpumask_any(cpu_online_mask);  /* valid online CPU */
  queue_work_on_emu(cpu, wq, &work);
  ```
- **VERIFICATION**: harness checks the item is placed on the requested CPU's
  pool and a second queue attempt to another CPU returns false.
- **SOURCE**: kernel-workqueue-docs (`queue_work_on` memory-ordering notes);
  kernel-driver-api.

## 3. `flush_work` semantics

- **RULE**: `flush_work(work)` sleeps until the work item has finished
  executing; the item is guaranteed idle on return if it has not been
  requeued since the flush started. Returns true if it waited, false if the
  item was already idle.
- **WHY AI GETS IT WRONG**: agents read flush as "cancel" (it is not), or
  assume it returns nothing useful, or forget it does nothing against a
  self-requeueing item.
- **CORRECT REASONING**: flush waits for completion of the last queueing
  instance; it does NOT remove a pending item — a pending item will still
  run, and flush waits for that run. If the work function re-queues itself,
  flush offers no guarantee. Use flush to order a shutdown after the item's
  effects, not to stop it.
- **EXAMPLE** (bad):
  ```c
  flush_work_emu(wq, &work);
  kfree(dev);             /* item may have re-queued itself: still pending */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (flush_work_emu(wq, &work))      /* waited for the last instance */
      ;                               /* resource safe only if no requeue */
  ```
- **VERIFICATION**: harness: flush on a pending item executes it and returns
  true; flush on an idle item returns false; a self-requeueing item defeats
  the guarantee.
- **SOURCE**: kernel-workqueue-docs (`flush_work` kernel-doc); kernel-source
  (kernel/workqueue.c `flush_work`).

## 4. `flush_workqueue` / `drain_workqueue`

- **RULE**: `flush_workqueue(wq)` sleeps until all work items queued on entry
  have finished; new items submitted afterwards are not waited for.
  `drain_workqueue(wq)` waits until the queue becomes empty, including items
  chained by running work.
- **WHY AI GETS IT WRONG**: agents use flush_workqueue where they need a
  per-item guarantee, or believe it freezes the queue against new submissions.
- **CORRECT REASONING**: flush is a snapshot: work queued after the flush
  starts can still be running when it returns (the doc: "not livelocked by
  new incoming ones"). Drain handles chain-queueing and is the stronger
  shutdown primitive. `destroy_workqueue` drains pending work first, but
  non-pending `delayed_work` must be cancelled before calling it.
- **EXAMPLE** (bad):
  ```c
  flush_workqueue_emu(wq);
  destroy_workqueue_emu(wq);   /* delayed_work timers may still fire */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  cancel_delayed_work_sync_emu(&dwork);
  drain_workqueue_emu(wq);
  destroy_workqueue_emu(wq);
  ```
- **VERIFICATION**: harness: an item queued after flush start is not waited
  for; drain waits for chained items.
- **SOURCE**: kernel-workqueue-docs (`__flush_workqueue`, `drain_workqueue`,
  `destroy_workqueue` kernel-docs); kernel-source.

## 5. `cancel_work` vs `cancel_work_sync`

- **RULE**: `cancel_work(work)` only removes a pending item and returns
  whether it did; a running item keeps running to completion. `cancel_work_sync`
  removes a pending item AND waits for a running one; on return the item is
  guaranteed not pending or executing on any CPU (absent racing enqueues).
- **WHY AI GETS IT WRONG**: agents pick `cancel_work` in teardown because it
  "compiles and returns true", believing the work can no longer run.
- **CORRECT REASONING**: the docs' contract: `cancel_work` is asynchronous
  and safe from any context (even IRQ); `cancel_work_sync` is the teardown
  guarantee, works even when the work re-queues itself or migrates, and must
  be called from a sleepable context (for non-BH workqueues). If the code
  frees anything the work touches, `cancel_work` is never enough.
- **EXAMPLE** (bad):
  ```c
  cancel_work_emu(wq, &work);     /* async: running instance continues */
  kfree(dev);                     /* UAF if the item was running/requeued */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  cancel_work_sync_emu(wq, &work); /* removes pending AND waits running */
  kfree(dev);
  ```
- **VERIFICATION**: harness: `cancel_work` on a running item returns false
  and the item completes; `cancel_work_sync` on a pending item removes it
  and it never executes.
- **SOURCE**: kernel-workqueue-docs (`cancel_work_sync` kernel-doc);
  kernel-source.

## 6. `cancel_work_sync` before free (use-after-free class)

- **RULE**: before freeing a `work_struct` — or the container/data its work
  function dereferences — the item must be guaranteed not pending or running:
  stop the producer, then `cancel_work_sync` (or flush), then free.
- **WHY AI GETS IT WRONG**: agents free the device/`work_struct` at teardown
  and rely on `cancel_work()` or on "nothing is queued right now".
- **CORRECT REASONING**: a pending item is still linked on the queue and will
  execute the freed work function (use-after-free); a running item is
  executing right now. `cancel_work_sync` gives the required guarantee, but
  only "as long as there aren't racing enqueues" — so the producer must also
  be stopped or the item is re-queued after the sync. The kernel even keys a
  busy-hash by the item address to detect recycled-work executions
  (`find_worker_executing_work`), which is a mitigation, not a license.
- **EXAMPLE** (bad):
  ```c
  queue_work_emu(wq, &dev->work);
  cancel_work_emu(wq, &dev->work);   /* racing producer re-queues */
  kfree(dev);                        /* item still PENDING -> UAF */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  dev->producer_enabled = false;     /* stop further queue_work */
  cancel_work_sync_emu(wq, &dev->work);
  kfree(dev);                        /* item guaranteed idle */
  ```
- **VERIFICATION**: harness: the bad sequence marks the item freed while
  PENDING and the runner detects "work ran after work_struct freed"; the good
  sequence leaves nothing pending and the runner finds nothing.
- **SOURCE**: kernel-workqueue-docs (cancel_work_sync guarantee); kernel-source
  (`find_worker_executing_work`, busy-hash); ldd3 (unload discipline).

## 7. Flush-from-work-context deadlock

- **RULE**: never call `flush_work` on the item you are currently executing,
  and never `flush_workqueue` (or drain) the workqueue whose worker you are
  running on. The worker would wait for its own completion — a self-deadlock.
- **WHY AI GETS IT WRONG**: agents add a `flush_work` before a free inside a
  work function "to be safe", not realizing the flusher is the item itself.
- **CORRECT REASONING**: flush waits on a completion that only the worker
  thread can signal; when the caller IS that worker, the completion never
  comes. The lockdep workqueue annotations exist precisely to report this
  class ("possible recursive locking" / WQ-flush warnings) before the hang.
  `cancel_work_sync` from inside the work function's own item is the same
  class of self-wait.
- **EXAMPLE** (bad):
  ```c
  static void dev_work(struct work_struct *work) {
      flush_work_emu(wq, work);      /* waits for itself: deadlock */
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static void dev_work(struct work_struct *work) {
      /* defer teardown bookkeeping; never flush the running item */
  }
  ```
- **VERIFICATION**: harness flags flush-on-running-item as the kernel's
  deadlock class; target: lockdep build reports it.
- **SOURCE**: kernel-workqueue-docs; kernel-lockdep-docs (workqueue lockdep
  annotations); kernel-source (`find_worker_executing_work` self-dependency
  note).

## 8. Work item reentrancy and self-requeue

- **RULE**: a work item never executes concurrently with itself: it is
  executed by at most one worker system-wide at any given time, provided the
  work function is unchanged, no one queues it to another workqueue, and it
  is not re-initialized. Re-queueing the item from its own function (to the
  same queue) is explicitly safe.
- **WHY AI GETS IT WRONG**: agents treat a self-rescheduling work item as
  unbounded or racy, or — worse — assume `cancel_work` stops a rescheduling
  item.
- **CORRECT REASONING**: self-requeue is the documented polling pattern; the
  item just runs again after the current execution finishes. It is exactly
  why `cancel_work_sync` exists: "can be used even if the work re-queues
  itself". Breaking the conditions (queueing to another workqueue, or
  changing the function) invalidates the non-reentrance guarantee.
- **EXAMPLE** (bad):
  ```c
  cancel_work_emu(wq, &work);   /* removed one pending instance */
  kfree(dev);                   /* the item will re-queue itself -> UAF */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  dev->polling = false;               /* stop the self-reschedule */
  cancel_work_sync_emu(wq, &work);    /* handles re-queueing items */
  kfree(dev);
  ```
- **VERIFICATION**: harness: a work function re-queues itself and runs twice;
  `cancel_work` leaves it re-pending, `cancel_work_sync` with polling off
  stops it.
- **SOURCE**: kernel-workqueue-docs (non-reentrance conditions, self-requeue
  note, cancel_work_sync).

## 9. Flush/cancel in the module unload sequence

- **RULE**: module exit must stop submission, drain or flush the workqueue,
  cancel any delayed work, then free the work items and the resources they
  touch — in that order — before returning.
- **WHY AI GETS IT WRONG**: agents free device data first and expect the
  workqueue to "go away with the module", or rely on `destroy_workqueue`
  alone.
- **CORRECT REASONING**: `destroy_workqueue` does pending work first, but it
  does NOT touch non-pending `delayed_work` (only linked on the timer side);
  those must be cancelled by the caller. Order: disable producer ->
  `cancel_delayed_work_sync` / `cancel_work_sync` -> `drain_workqueue` or
  `destroy_workqueue` -> free the work_structs. A module unloaded while a
  kworker still executes its code oopses on the first module memory access.
- **EXAMPLE** (bad):
  ```c
  static void __exit mod_exit(void) {
      kfree(dev);              /* work may still be queued/running */
      destroy_workqueue_emu(wq); /* too late */
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static void __exit mod_exit(void) {
      dev->open = false;                  /* stop producers */
      cancel_delayed_work_sync_emu(&dev->dwork);
      cancel_work_sync_emu(wq, &dev->work);
      drain_workqueue_emu(wq);
      destroy_workqueue_emu(wq);
      kfree(dev);
  }
  ```
- **VERIFICATION**: harness: the good sequence leaves no pending/running item
  at free time; the bad sequence trips the emulated use-after-free.
- **SOURCE**: ldd3 (module unload discipline, deferred work); kernel-workqueue-docs
  (`destroy_workqueue`, `drain_workqueue`); kernel-driver-api.

## 10. Ordered workqueues

- **RULE**: `alloc_ordered_workqueue()` executes at most one work item at a
  time, in queued order (implemented as an unbound workqueue with
  `max_active` 1). Do not emulate ordering with `max_active = 1` + `WQ_UNBOUND`
  or with plain bound workqueues.
- **WHY AI GETS IT WRONG**: agents rely on `max_active = 1` and a bound
  workqueue to get ordering and get per-CPU parallel execution instead.
- **CORRECT REASONING**: only an ordered workqueue gives strict
  one-at-a-time FIFO semantics; `max_active` limits concurrency, not
  ordering. If a shutdown path flushes an ordered wq, items already on it run
  in submission order, which callers may depend on.
- **EXAMPLE** (bad):
  ```c
  wq = alloc_workqueue_emu("dev", WQ_UNBOUND, 1); /* not ordered */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  wq = alloc_ordered_workqueue_emu("dev", 0);
  ```
- **VERIFICATION**: harness queues two items and checks the second never
  runs before the first on the ordered queue.
- **SOURCE**: kernel-workqueue-docs (`alloc_ordered_workqueue`, `max_active`
  notes).

## 11. Work items hold no reference on the module

- **RULE**: queuing work does not pin the module or the device: a work item
  is a pointer to a function in module text. The module must guarantee the
  code and data remain valid while the item may execute — by draining the
  queue in the exit path.
- **WHY AI GETS IT WRONG**: agents assume the module loader or the
  workqueue subsystem protects them, or that `.owner = THIS_MODULE` covers
  work items.
- **CORRECT REASONING**: `.owner` protects the module from sysfs/file
  operations being unloaded, not from deferred execution. A kworker already
  mid-execution (or about to dequeue the item) does not hold the module
  reference; the unload path must flush/cancel and destroy the workqueue
  before the module text goes away. The same rule applies to freeing the
  device structure the work function dereferences.
- **EXAMPLE** (bad):
  ```c
  static struct workqueue_struct *wq;   /* no reference counting */
  /* .owner set, but the exit path never drains wq */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static void __exit mod_exit(void) {
      /* drain + destroy before module text is unmapped */
      drain_workqueue_emu(wq);
      destroy_workqueue_emu(wq);
  }
  ```
- **VERIFICATION**: harness: items still queued at "unload" execute after
  the free unless the queue is drained first.
- **SOURCE**: ldd3 (module unload); kernel-driver-api (driver lifecycle);
  kernel-workqueue-docs.

## 12. Interrupts / BH context: work runs in process context

- **RULE**: normal (threaded) work items execute in process context on
  kworker threads and may sleep. `cancel_work_sync` must be called from a
  sleepable context for non-BH workqueues; it cannot run in hard-IRQ or with
  a spinlock held that the work function takes. Work may also be queued from
  IRQ/BH context — that side is cheap and safe — but the sync/cancel side has
  the sleep requirement.
- **WHY AI GETS IT WRONG**: agents call `cancel_work_sync` from an interrupt
  handler or under a spinlock "to stop the work immediately".
- **CORRECT REASONING**: the docs: `cancel_work_sync` "must be called from a
  sleepable context" (for non-BH workqueues) and must not be called with any
  lock held that the work function needs — otherwise the work function blocks
  on the lock while the sync path waits for it (deadlock, lockdep-detectable).
  `cancel_work` is the context-free variant: safe from IRQ, but async.
- **EXAMPLE** (bad):
  ```c
  spin_lock_irqsave(&dev->lock, flags);
  cancel_work_sync_emu(wq, &dev->work);  /* sleeps in atomic context */
  spin_unlock_irqrestore(&dev->lock, flags);
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  cancel_work_emu(wq, &dev->work);       /* async: safe from IRQ */
  /* real teardown moves to process context: cancel_work_sync there */
  ```
- **VERIFICATION**: harness: an item "runs" after cancel_work from IRQ
  context; only the process-context `cancel_work_sync` guarantees idle.
- **SOURCE**: kernel-workqueue-docs (`cancel_work_sync` context note);
  kernel-lockdep-docs (lock-deadlock detection); ldd3.

## Quick detection table

| Pattern | Class | Check |
|---|---|---|
| `kfree` after `queue_work` without flush/cancel | UAF | need `cancel_work_sync` before free |
| `cancel_work()` then `kfree` | UAF | racing enqueue / running instance |
| `flush_work` inside its own work function | deadlock | never flush the running item |
| `flush_workqueue` on the wq you run on | deadlock | self-wait on the worker |
| `cancel_work_sync` in IRQ/BH or with spinlock | sleep-in-atomic / deadlock | process context only |
| `cancel_work` relied on for teardown | async-only | use `cancel_work_sync` + stop producer |
| module exit without drain/destroy | UAF after unload | drain, destroy, then free |
| `delayed_work` not cancelled before destroy | late timer | `cancel_delayed_work_sync` first |
| self-requeue item stopped only by `cancel_work` | UAF | polling flag off + `cancel_work_sync` |
| queueing to a second workqueue while running | reentrancy break | keep one queue per item |
