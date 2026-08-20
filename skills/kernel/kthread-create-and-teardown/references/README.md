# Linux Kernel Thread Rules

Source-backed rule set for the kthread create/stop lifecycle. Each entry:
RULE -> WHY AI GETS IT WRONG -> CORRECT REASONING -> EXAMPLE -> COUNTEREXAMPLE
-> VERIFICATION -> SOURCE. Confidence markers: KNOWN (documented contract),
INFERRED (derived), UNVERIFIED (never use in a stable skill).

## 1. `kthread_create` returns a task_struct* that is NOT running

- **RULE**: `kthread_create(threadfn, data, namefmt, ...)` allocates a
  `task_struct` for a kernel thread but does NOT start it. The threadfn does
  not execute until the task is woken with `wake_up_process()` (or via
  `kthread_run`).
- **WHY AI GETS IT WRONG**: a returned task pointer reads like a running
  thread; agents assume the threadfn has already consumed `data`.
- **CORRECT REASONING**: creation is passive. Until a wake, the task sits in
  `TASK_UNINTERRUPTIBLE` under kthreadd and the threadfn never ran. Any
  code that touches threadfn-owned state right after `kthread_create` races
  with nothing — the thread simply has not started.
- **EXAMPLE** (bad):
  ```c
  task = kthread_create(worker, &ctx, "my-thread");
  ctx.queue_len = 5;   /* thread may never have run; ordering assumed */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  task = kthread_create(worker, &ctx, "my-thread");
  if (IS_ERR(task))
      return PTR_ERR(task);
  wake_up_process(task);   /* or use kthread_run() outright */
  ```
- **VERIFICATION**: harness: `kthread_create_emu` leaves `state ==
  KT_ST_CREATED` and `steps == 0` until the scheduler runs a step.
- **SOURCE**: kernel-driver-api (kthread_create kernel-doc); kernel-source
  (kernel/kthread.c create path).

## 2. `kthread_run` = `kthread_create` + `wake_up_process`

- **RULE**: `kthread_run(threadfn, data, namefmt, ...)` is the create-and-
  wake shortcut: it creates the task and wakes it, so after it returns the
  threadfn is executing (or at least schedulable).
- **WHY AI GETS IT WRONG**: agents call `kthread_create` and forget the
  wake, then debug "the thread never ran".
- **CORRECT REASONING**: `kthread_run` exists precisely because the
  create-then-wake pair is the common case. Use it unless you need the
  `task_struct*` before waking (e.g. to park it first). It returns an
  `ERR_PTR` on failure — always check with `IS_ERR`.
- **EXAMPLE** (bad):
  ```c
  task = kthread_create(worker, &ctx, "w");
  if (IS_ERR(task))
      return PTR_ERR(task);
  /* never woken: thread sits parked forever */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  task = kthread_run(worker, &ctx, "w");
  if (IS_ERR(task))
      return PTR_ERR(task);
  ```
- **VERIFICATION**: harness: `kthread_run_emu` leaves `state ==
  KT_ST_RUNNING` and has executed at least one step; create alone has none.
- **SOURCE**: kernel-driver-api (kthread_run kernel-doc); kernel-source
  (kernel/kthread.c `kthread_create_on_node` + wake in kthread_run).

## 3. threadfn must loop and poll `kthread_should_stop()`

- **RULE**: the threadfn's main loop must call `kthread_should_stop()`
  (which reads the current task's stop flag) and `return` when it becomes
  true. That return is the thread's exit and becomes `kthread_stop`'s value.
- **WHY AI GETS IT WRONG**: agents write `while (1)` or `for (;;)` plus a
  `wait_event` that never considers stop, so the thread can never be
  ended cooperatively.
- **CORRECT REASONING**: the stop flag is polled, not pushed. The kernel
  cannot preempt the threadfn into exiting; it only sets the flag, wakes the
  task, and waits. The threadfn must notice and return. Wait conditions must
  include the stop flag: `wait_event(q, cond || kthread_should_stop())`.
- **EXAMPLE** (bad):
  ```c
  while (1) {
      wait_event(wq, item_ready);
      process_one();
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  while (!kthread_should_stop()) {
      wait_event(wq, item_ready || kthread_should_stop());
      if (item_ready)
          process_one();
  }
  return 0;
  ```
- **VERIFICATION**: harness: a threadfn that never polls cannot reach
  `KT_ST_EXITED` when stop is requested (the step loop forces exit only at
  the contract boundary).
- **SOURCE**: kernel-kthread-docs (kthread_should_stop contract);
  kernel-source (kernel/kthread.c).

## 4. `kthread_stop` sets the flag and waits — live task, sleepable context

- **RULE**: `kthread_stop(task)` sets the stop flag, wakes the task, and
  BLOCKS until the threadfn returns and the task exits (goes through
  `do_exit`). It must be called only while the task is alive and only from a
  sleepable (process) context.
- **WHY AI GETS IT WRONG**: treated as an async "please stop" hint, or
  called from timers/interrupts/atomic paths where sleeping is illegal.
- **CORRECT REASONING**: `kthread_stop` is a blocking join, not a signal.
  It waits inside, so it needs process context and the task must still
  exist. Stopping from atomic context sleeps in a forbidden place; stopping
  a dead task dereferences state that may be gone.
- **EXAMPLE** (bad):
  ```c
  hrtimer_cb(...) { kthread_stop(task); }   /* sleeps in atomic */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  /* process context, task provably alive */
  int ret = kthread_stop(task);
  ```
- **VERIFICATION**: harness: `kthread_stop_emu` on a live task runs the
  thread to `KT_ST_EXITED` and returns the threadfn's result.
- **SOURCE**: kernel-kthread-docs (kthread_stop contract);
  kernel-source (kernel/kthread.c `kthread_stop`).

## 5. `kthread_stop` returns the threadfn's return value

- **RULE**: the value returned by `kthread_stop()` is exactly the value the
  threadfn returned when it exited.
- **WHY AI GETS IT WRONG**: the return is ignored, or assumed to be an error
  code, or read from a global written by the thread.
- **CORRECT REASONING**: the threadfn's `int` return travels through the
  exit path and becomes `kthread_stop`'s return. Use it to propagate a
  result or error from the thread to the teardown path.
- **EXAMPLE** (bad):
  ```c
  kthread_stop(task);               /* thread's error status lost */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int ret = kthread_stop(task);
  if (ret < 0)
      dev_err(dev, "worker failed: %d\n", ret);
  ```
- **VERIFICATION**: harness: worker returns its processed sum on stop;
  `kthread_stop_emu` returns 67 and the assertion passes.
- **SOURCE**: kernel-source (kernel/kthread.c, threadfn return propagation);
  kernel-driver-api (kthread_stop kernel-doc).

## 6. `kthread_stop` on an already-exited task is a bug

- **RULE**: calling `kthread_stop()` on a task that has already exited is a
  documented contract violation. The caller must know the task is still
  alive; after exit, the task is gone and the stop is a use-after-exit race.
- **WHY AI GETS IT WRONG**: a defensive "stop it just in case" placed after
  the thread may already have returned (one-shot threadfn, or a race where
  the thread exited on its own).
- **CORRECT REASONING**: `kthread_stop` is only valid while the task lives.
  Liveness is the caller's job: either the threadfn only ever exits via
  `kthread_should_stop()` (so stop is always legal), or the caller proves
  liveness with a completion the threadfn signals before returning.
- **EXAMPLE** (bad):
  ```c
  /* one-shot threadfn returns after the first request */
  kthread_stop(task);   /* task may already be gone */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  /* threadfn loops until kthread_should_stop(); stop is always valid */
  kthread_stop(task);
  ```
- **VERIFICATION**: harness: `kthread_stop_emu` on a `KT_ST_EXITED` task
  prints "BUG reproduced: kthread_stop on exited task".
- **SOURCE**: kernel-kthread-docs (kthread_stop contract);
  kernel-source (kernel/kthread.c).

## 7. Module unload ordering: stop the thread first, then free resources

- **RULE**: on module unload the kthread must be stopped BEFORE the driver
  frees any memory, device, or file state the threadfn can touch. Order:
  `kthread_stop()` -> free resources.
- **WHY AI GETS IT WRONG**: the instinctive order is cleanup-then-join
  (`kfree(ctx); kthread_stop(task);`) — exactly inverted for kthreads.
- **CORRECT REASONING**: the threadfn can be mid-access at any instant
  between steps; it only exits when it returns. `kthread_stop` is the join
  that guarantees the thread is no longer touching anything, so freeing must
  come after the join returns. Freeing first gives a use-after-free race
  that usually shows up only under load or on real hardware.
- **EXAMPLE** (bad):
  ```c
  kfree(ctx);            /* thread may still be reading ctx */
  kthread_stop(task);
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  kthread_stop(task);    /* thread is gone; now it is safe */
  kfree(ctx);
  ```
- **VERIFICATION**: harness: freeing while `state == KT_ST_RUNNING` prints
  "BUG reproduced: resources freed before kthread_stop".
- **SOURCE**: kernel-driver-api (teardown conventions); ldd3 (driver
  lifecycle: cleanup in the reverse order of setup).

## 8. A kthread that exits by itself is a caller design bug

- **RULE**: a kthread should run until `kthread_should_stop()` becomes true.
  If the threadfn can return on its own (job done), the caller cannot know
  when a later `kthread_stop` is safe, so it must synchronize — otherwise
  the design is wrong.
- **WHY AI GETS IT WRONG**: one-shot threadfn ("do one thing, return") plus
  an unconditional `kthread_stop` in `module_exit`.
- **CORRECT REASONING**: self-exit and `kthread_stop` race by construction.
  The clean pattern is a threadfn that loops polling the stop flag and
  returns only when stopped; then `kthread_stop` is always legal. If a
  self-exiting thread is truly needed, pair it with an exit completion and
  `wait_for_completion` before stopping — and do not `kthread_stop` a task
  you know exited.
- **EXAMPLE** (bad):
  ```c
  static int once(void *data) { handle(data); return 0; }  /* self-exits */
  /* ... later ... */
  kthread_stop(task);   /* race: task may already be gone */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static int loop(void *data)
  {
      while (!kthread_should_stop()) { ... }
      return 0;
  }
  ```
- **VERIFICATION**: harness: the `one_shot` threadfn reaches `KT_ST_EXITED`
  alone; a following `kthread_stop_emu` is flagged.
- **SOURCE**: kernel-kthread-docs (kthread lifecycle guidance).

## 9. `kthread_park` / `kthread_unpark` suspend without destroying

- **RULE**: `kthread_park()` suspends a running kthread (its threadfn stops
  being scheduled) and `kthread_unpark()` resumes it. Parking is NOT
  termination: the task stays alive and can be stopped later.
- **WHY AI GETS IT WRONG**: `kthread_stop` is used to "pause" a thread and
  a new kthread is spawned to resume — destroying and recreating state.
- **CORRECT REASONING**: park/unpark is the suspend/resume pair. A parked
  thread is alive but not running; it cannot exit while parked, so it must
  be unparked before `kthread_stop` (and a parked thread's `kthread_stop`
  waits for the unpark).
- **EXAMPLE** (bad):
  ```c
  kthread_stop(task);              /* "pause" */
  task = kthread_run(worker, &ctx, "w");   /* recreate to "resume" */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  kthread_park(task);     /* suspend */
  kthread_unpark(task);   /* resume, same task, same state */
  ```
- **VERIFICATION**: review: park/unpark on the same `task_struct`, no
  create/stop churn; stop only after unpark.
- **SOURCE**: kernel-driver-api (kthread_park/unpark kernel-doc);
  kernel-source (kernel/kthread.c park machinery).

## 10. Completion handshake: prove the thread is running before `kthread_stop`

- **RULE**: make `kthread_stop` safe with a completion handshake: the
  threadfn signals a completion when it has started (and optionally when it
  exits); the caller waits on the "started" completion before stopping, so
  the thread is provably alive.
- **WHY AI GETS IT WRONG**: `kthread_stop` right after `kthread_run` with no
  liveness proof — fine in demos, racy in production (thread may not have
  started, or may have already finished).
- **CORRECT REASONING**: the stop-while-alive contract needs evidence.
  `init_completion` + `complete()` in the threadfn + `wait_for_completion()`
  in the caller gives the happens-before edge: after the wait, the threadfn
  has run at least past its start marker, so `kthread_stop` targets a live
  task. For self-exiting threads, a second "exited" completion removes the
  race entirely (check it before stopping).
- **EXAMPLE** (bad):
  ```c
  task = kthread_run(worker, &ctx, "w");
  kthread_stop(task);      /* no proof the thread started or is alive */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  task = kthread_run(worker, &ctx, "w");
  wait_for_completion(&ctx.started);   /* threadfn did complete(&started) */
  kthread_stop(task);
  ```
- **VERIFICATION**: harness: the good example asserts `state ==
  KT_ST_RUNNING` (alive) before calling `kthread_stop_emu`.
- **SOURCE**: kernel-kthread-docs (lifecycle guidance); kernel-source
  (kernel/kthread.c, completion usage in kthread users).

## 11. Kthread data lifetime: no module refcount

- **RULE**: a kthread holds NO reference on the module. `module_exit` can
  run while the kthread is still executing the driver's code, so the module
  must stop the thread before its code or data disappears.
- **WHY AI GETS IT WRONG**: assumes the running thread pins the module (only
  the threadfn's `task_struct` is refcounted, not the module's code/data).
- **CORRECT REASONING**: like a workqueue worker, the kthread executes the
  driver's function in the module's address space with no refcount guard.
  Unloading while the thread runs means executing code that is being freed —
  a crash or silent corruption. Teardown must be explicit, ordered, and
  complete in `module_exit`: stop the thread, free its data, then let the
  module go.
- **EXAMPLE** (bad): returning from `module_exit` without `kthread_stop`,
  expecting the thread to "clean itself up".
- **COUNTEREXAMPLE** (good):
  ```c
  static void __exit driver_exit(void)
  {
      kthread_stop(dev->thread);   /* thread is gone */
      kfree(dev->ctx);             /* now safe to free */
      unregister_*_device(...);
  }
  ```
- **VERIFICATION**: harness: stop-before-free ordering asserted on the
  ktask's data sentinel; target: load/unload loop under KASAN.
- **SOURCE**: kernel-kthread-docs (module teardown guidance); ldd3 (driver
  teardown conventions).

## 12. Prefer workqueues over kthreads for short-lived jobs

- **RULE**: use a workqueue (`queue_work` / `schedule_work` plus
  `cancel_work_sync` / `flush_work`) for bounded background jobs; use a
  kthread only for a dedicated long-lived context — its own stack, priority,
  per-CPU affinity, or a persistent polling loop.
- **WHY AI GETS IT WRONG**: a kthread is spawned per request "to do the work
  asynchronously", then the agent must maintain the whole lifecycle
  (create, wake, handshake, stop) that workqueues provide for free.
- **CORRECT REASONING**: workqueues give deferred execution, concurrency
  limits, and ready-made flush/cancel semantics with safe refcounting.
  Kthreads add manual teardown and liveness bookkeeping; if the job is
  bounded or queue-driven, a workqueue is the correct primitive.
- **EXAMPLE** (bad): creating a kthread inside every ioctl to do a small
  async job.
- **COUNTEREXAMPLE** (good):
  ```c
  INIT_WORK(&dev->job, job_work);
  queue_work(system_wq, &dev->job);
  /* teardown: cancel_work_sync(&dev->job); */
  ```
- **VERIFICATION**: review: does the code need a persistent, named,
  independently-scheduled context? If not, use a workqueue.
- **SOURCE**: ldd3 (deferred work, workqueues); kernel-kthread-docs.

## Quick detection table

| Pattern | Class | Check |
|---|---|---|
| resources freed before `kthread_stop` | UAF / order inversion | stop first, free after |
| `kthread_stop` on an exited task | use-after-exit race | prove liveness (completion) |
| threadfn without `kthread_should_stop` poll | unload hang | poll each loop iteration |
| `kthread_stop` from atomic/timer context | sleep-in-atomic | call from process context |
| wake of a stopped kthread | use-after-death | never wake after stop |
| `kthread_create` without wake | thread never runs | use `kthread_run` |
| unload without stopping the thread | freed-module code running | stop in `module_exit` first |
| per-request kthread | lifecycle overkill | use a workqueue |
