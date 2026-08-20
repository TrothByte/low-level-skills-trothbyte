---
name: workqueue-flush-and-cancellation
description: Use when writing, reviewing, or debugging Linux kernel code that queues work and must shut down safely — queue_work/queue_work_on, flush_work/flush_workqueue, cancel_work_sync vs cancel_work, work item reentrancy and rescheduling, module unload, and the flush-from-work-context deadlock. Teaches the workqueue flush/cancel contract that prevents use-after-free.
---

# Workqueue Flush & Cancellation

The Linux workqueue contract for shutting down deferred work without
use-after-free or deadlock. Load `references/README.md` before writing any
`queue_work`/`flush_work`/`cancel_work_sync` teardown path.

## When to use

- Writing or reviewing driver/subsystem code that defers work with
  `queue_work` / `queue_work_on` (or `schedule_work`).
- Teardown paths — `.remove`, module unload, `.release` — where a `work_struct`
  or the resources its function touches are about to be freed.
- Deciding between `flush_work`, `flush_workqueue`, `cancel_work`, and
  `cancel_work_sync`, or reviewing an existing call.
- Work items that reschedule themselves (self-requeue polling pattern).
- Debugging unload hangs, kworker use-after-free reports, or lockdep
  "recursive locking" / WQ-flush reports.

## When not to use

- Code with no deferred work at all (no `work_struct`).
- User-space thread pools; the ownership model differs.
- Other deferred-execution mechanisms with different cancel rules:
  tasklets/softirq, hrtimers/timers, dedicated kthreads.
- Micro-tuning a correct flush/cancel path; this skill is about correctness.

## What the agent often gets wrong

- Believing `cancel_work()` stops a running item. It only removes a pending
  one; a running item keeps running to completion.
- Freeing the `work_struct` (or the data its function dereferences) right
  after `queue_work` or after `cancel_work()` — without `cancel_work_sync()` —
  leaving a pending/running item to touch freed memory.
- Calling `flush_work()` (or `flush_workqueue()`) from inside the very work
  item the flush is waiting on: the worker waits for itself — deadlock.
- Treating `flush_work` as cancellation. It waits for completion; it does not
  remove a pending item, and it gives no guarantee if the item self-requeues.
- Believing a running item can be re-queued freely: `queue_work` returns
  false for a pending item, and only the item's own function may re-queue it.
- Returning from module unload while a work item may still execute module code.
- Calling `cancel_work_sync()` while holding a lock the work function takes,
  or from an atomic/BH/IRQ context where it cannot sleep.

## How to reason correctly

1. Model each work item as `IDLE -> PENDING -> RUNNING -> IDLE`. It is on at
   most one queue at a time; `queue_work` performs only `IDLE -> PENDING` and
   returns false otherwise.
2. `queue_work` queues on the issuing CPU; `queue_work_on(cpu, ...)` pins the
   item to a chosen CPU (bound per-CPU workqueues) for locality/affinity.
3. While RUNNING, an item cannot be queued by another caller; only its own
   work function may re-queue it (self-reschedule), which the docs permit and
   which is the standard polling pattern.
4. `flush_work(work)` waits until the item is neither pending nor running.
   Returns true if it waited, false if already idle. Does not cancel.
5. `flush_workqueue(wq)` / `drain_workqueue(wq)` wait for all work on the wq;
   flush covers items queued at entry, drain also waits chain-queued items.
6. `cancel_work()` is async: it dequeues a pending instance and reports
   whether it did. `cancel_work_sync()` removes a pending item AND waits for a
   running one; on return the item is not pending or executing (absent racing
   enqueues).
7. Teardown rule: stop the producer (no further `queue_work` possible), then
   `cancel_work_sync()` (or flush), then free. Nothing may run after the free.
8. Never flush your own item: `flush_work` on the item you are executing, or
   `flush_workqueue` on the wq whose worker you are, is a self-wait deadlock.
9. Work items hold no reference on the module: the module must drain and
   destroy its workqueue (and cancel delayed work) before unloading.

## What to verify

- Every `kfree()` of a `work_struct`, or of the container/resources its
  function touches, is preceded by `cancel_work_sync()` (or flush) with the
  producer disabled.
- No `flush_work`/`flush_workqueue` reachable from the work function's own
  execution path.
- `cancel_work_sync()` only from process/sleepable context, never with a lock
  the work function takes.
- Module exit drains and destroys the workqueue before returning.
- `queue_work_on` CPU argument is valid.
- `delayed_work` timers cancelled before `destroy_workqueue`.
- A self-requeueing item is stopped only by `cancel_work_sync` + producer stop.

## How to verify

Host-compilable logic checks (self-contained stubs, no kernel headers):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_flush.c -o /tmp/good_flush
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_flush.c -o /tmp/bad_flush
```

Target (kernel) checks — document these, do not claim to have run them:

```
# lockdep: CONFIG_LOCKDEP build; the self-flush deadlock is reported by the
# workqueue lockdep annotations ("possible recursive locking") before the hang
# KASAN VM: CONFIG_KASAN under QEMU; free-then-run UAF is reported on the
# kworker access (use-after-free)
```

## Where the knowledge comes from

- `kernel-workqueue-docs` — Documentation/core-api/workqueue.rst: the
  queue_work/flush_work/cancel_work_sync contracts and non-reentrance rules
- `kernel-source` — kernel/workqueue.c: implementations, the flush completion
  mechanism, `find_worker_executing_work()` busy-hash recycled-work detection
- `kernel-driver-api` — driver API docs: workqueue lifecycle in drivers,
  `devm_alloc_workqueue` auto-destroy on detach
- `ldd3` — deferred work chapter: workqueues vs tasklets, unload discipline
- `kernel-lockdep-docs` — lockdep workqueue flush/cancel annotations that
  catch flush-from-work-context and lock-inversion deadlocks

## Related skills

- `kernel-driver-char-device-lifecycle` — require: teardown ordering
- `kernel-atomic-context` — require: where `cancel_work_sync` may sleep
- `kthread-create-and-teardown` — recommend: alternative deferral model
- `kernel-timers-hrtimer-vs-legacy` — recommend: timer deferral and cancel
- `deadlock-kernel-prevention` — recommend: self-wait, ordering, lockdep
- `kernel-uaccess-safety` — recommend: work functions touching user buffers

## Evaluation

Workqueue flush/cancel bugs generally have no public CVE; they are documented
bug classes fixed per-driver by commit. Two classes:

1. Use-after-free — `cancel_work_sync()`/`flush_work()` missing before
   `kfree()` of the `work_struct` or the resources its function dereferences;
   a pending or running item executes after the free. Documented in
   `kernel-workqueue-docs` (cancel_work_sync guarantee) and defended against
   by the `find_worker_executing_work()` busy-hash in `kernel-source`.
2. Self-flush deadlock — `flush_work()`/`flush_workqueue()` called from within
   the same work item waits for the running worker, which waits for itself.
   Documented class; surfaced by the lockdep WQ annotations
   (`kernel-lockdep-docs`). The cmwq rework (Tejun Heo, merged v2.6.36)
   introduced the modern flush/cancel semantics these docs describe.

No CVE numbers are assigned or claimed for either class. Synthetic evals:
correct teardown must not be flagged; kfree-after-queue_work,
kfree-after-cancel_work-only, self-flush, and atomic-context cancel_work_sync
must be flagged. Adversarial: a self-rescheduling item that makes
`cancel_work` "succeed" yet keeps running; "flush_work then kfree" with a live
producer; `cancel_work_sync` from hard-IRQ context. False-positive: proper
teardown (stop producer, cancel_work_sync, free, verify idle) must NOT be
flagged.
