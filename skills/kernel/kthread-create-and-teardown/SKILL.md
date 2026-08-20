---
name: kthread-create-and-teardown
description: Use when writing, reviewing, or debugging Linux kernel threads — kthread_create/kthread_run, the kthread_should_stop/kthread_stop contract, threadfn return, module unload ordering, and races between kthread_stop and thread exit. Teaches the create/stop lifecycle so threads never outlive their resources.
---

# Kernel Threads: Create & Teardown

Rules for the kthread create/stop lifecycle: what `kthread_create` vs
`kthread_run` start, what `kthread_stop` really does (it sets a flag and
waits), and in which order module teardown must happen. Load
`references/README.md` before writing any kthread code or unload path.

## When to use

- Creating or reviewing a long-lived kernel thread
  (`kthread_create` / `kthread_run`).
- Writing the threadfn: its poll loop, return value, and teardown.
- Shutting a thread down (`kthread_stop`) from ioctl, file close, or
  `module_exit`.
- Ordering module unload: stopping the thread vs freeing its resources.
- Debugging hangs, UAFs, or "thread kept running after driver died" bugs.

## When not to use

- One-shot or bounded background jobs: use a workqueue and
  `cancel_work_sync` / `flush_work`.
- User-space threads (`kthreadd` is kernel-only; use `kthread_create` only
  in kernel context).
- Other kernels: Windows (`PsCreateSystemThread`), BSD/Darwin
  (`kthread_create` with different stop semantics) differ.
- Anything where a workqueue gives flush/cancel for free.

## What the agent often gets wrong

- Believing `kthread_create` starts the thread; it only allocates the task,
  the threadfn runs only after a wake (`kthread_run` = create + wake).
- Treating `kthread_stop` as a fire-and-forget kill. It sets a stop flag,
  then BLOCKS until the threadfn returns and the task exits.
- Freeing the thread's data and then calling `kthread_stop` — the exact
  inverse of the required order (stop first, free after).
- Writing a threadfn loop that never polls `kthread_should_stop()`, so
  `kthread_stop` hangs forever on unload.
- Calling `kthread_stop` from atomic context (it sleeps).
- Calling `kthread_stop` on a task that already exited on its own.
- Waking a stopped kthread (`kthread_wake` after stop) — the task is dead.

## How to reason correctly

1. Decide who starts the thread. `kthread_create` alone means "not running";
   add `wake_up_process` or use `kthread_run`. Never touch the threadfn's
   data assuming it has started.
2. The threadfn is the thread. Its main loop must poll
   `kthread_should_stop()` and `return` when it becomes true — that return
   is the thread's exit.
3. `kthread_stop(task)` sets the stop flag, wakes the task, and waits for it
   to exit. It may only be called while the task is alive and only from a
   sleepable (process) context.
4. `kthread_stop` returns the threadfn's return value; use it to propagate
   the thread's result or error.
5. Order teardown: stop the thread BEFORE freeing any memory or device
   resource the threadfn can touch. Stop-first, free-later, always.
6. If the thread can ever exit on its own, the caller cannot know when
   `kthread_stop` is safe — prove liveness with a completion the threadfn
   signals (started / exited), or design the threadfn to only exit via
   `kthread_should_stop()`.
7. The kthread holds no module reference. The module must not unload while
   the thread runs; `module_exit` must stop the thread first.
8. For temporary suspension use `kthread_park` / `kthread_unpark`, not stop.

## What to verify

- The threadfn polls `kthread_should_stop()` in its main loop and returns
  when it is true.
- `kthread_stop` is called only on a live task (completion-proven) and only
  from process context.
- `module_exit` order: `kthread_stop` happens before any `kfree`/`unregister`
  of resources the threadfn touches.
- No `kthread_wake`/`kthread_stop` after the thread has exited.
- The thread's data outlives the thread: freed only after stop returns.
- No kthread is created "per request" where a workqueue would do.

## How to verify

Host-compilable lifecycle checks (self-contained stubs, no kernel headers):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_kthread.c -o /tmp/good_kthread
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_kthread.c -o /tmp/bad_kthread
```

Target (kernel) checks — document these, do not claim to have run them:

```
# module load/unload loop with the kthread; KASAN + lockdep enabled
# kthread_stop() in module_exit; thread's exit verified via
# /proc/PID/task/TID/stat after stop (state Z should never persist)
```

## Where the knowledge comes from

- `kernel-kthread-docs` — kthread API, kthread_stop contract, thread
  lifecycle and module teardown guidance
- `kernel-driver-api` — kthread_create/run/stop/park kernel-doc
- `kernel-source` — `kernel/kthread.c` implementation: stop flag, wait,
  threadfn return, wake paths
- `ldd3` — driver lifecycle and deferred-work conventions (workqueue vs
  kthread framing)

## Related skills

- `waitqueue-completion-synchronization` (require) — the liveness
  handshake that makes `kthread_stop` safe
- `workqueue-flush-and-cancellation` (recommend) — prefer workqueues for
  short-lived jobs
- `kernel-atomic-context` (require) — `kthread_stop` sleeps; never call it
  from atomic context
- `kernel-timers-hrtimer-vs-legacy` (recommend) — timer callbacks must not
  stop kthreads
- `kernel-driver-char-device-lifecycle` (recommend) — module teardown
  ordering for the driver that owns the thread
- `deadlock-kernel-prevention` (recommend) — stop-before-free ordering and
  join waits

## Evaluation

Documented kernel bug classes (from `kernel-kthread-docs`; no invented
CVE numbers):

- `kthread_stop` called on a task that already exited — documented contract
  violation (only valid while the task is alive).
- Module unload without stopping the thread first — the thread keeps
  touching freed driver memory after `module_exit` frees it.
- threadfn that never checks `kthread_should_stop()` — `kthread_stop`
  waits forever for a thread that will not exit.
- Waking a stopped kthread (`kthread_wake` after stop) — the task is dead;
  the wake is a use-after-death race.

Synthetic: free-before-stop order inversion, stop-after-exit, threadfn loop
without a stop poll, missing unload teardown. Adversarial: code that "passes"
because the module is never unloaded in the test, or a thread that exits on
its own while the driver still `kthread_stop`s it. False-positive: stop after
a completion proves the thread is alive, and resources freed only after
`kthread_stop` returns — must NOT be flagged.
