# Linux Kernel Waitqueue & Completion Rules

Source-backed rule set for blocking kernel synchronization. Each entry:
RULE -> WHY AI GETS IT WRONG -> CORRECT REASONING -> EXAMPLE ->
COUNTEREXAMPLE -> VERIFICATION -> SOURCE. Confidence markers: KNOWN
(documented contract), INFERRED (derived), UNVERIFIED (never use in a
stable skill).

## 1. `wait_event` re-checks the condition after every wake

- **RULE**: `wait_event(wq, condition)` sleeps until `condition` is true.
  The macro evaluates the condition before parking and re-evaluates it
  after every wake, in a loop. That re-check is what makes the wakeup
  race-free.
- **WHY AI GETS IT WRONG**: agents treat the wakeup itself as the event
  and write `while (!flag) sleep();` with no condition loop, or assume a
  single wakeup means the condition holds.
- **CORRECT REASONING**: the waitqueue only parks the waiter. The
  condition is the event; the loop re-checks it after each wake so a
  wakeup that raced with the condition store (or a spurious wake) never
  makes the waiter proceed on stale state.
- **EXAMPLE** (bad):
  ```c
  /* busy-wait: burns CPU, and on SMP races with the condition store */
  while (!data_ready)
      cpu_relax();
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  wait_event(wq, data_ready);
  ```
- **VERIFICATION**: harness: waker sets the flag between the waiter's
  first check and its park; the wait_event emulator still observes the
  flag and returns. A bare pre-check loop misses the event.
- **SOURCE**: kernel-source (kernel/wait.c, wait_event macro), ldd3
  (blocking I/O).

## 2. Condition must be set BEFORE `wake_up` (lost wakeup)

- **RULE**: the producer stores the condition and then calls `wake_up`.
  Setting the condition after the wake reopens the lost-wakeup race: a
  waiter that checked just before the store sleeps while the waker has
  already passed the wake point.
- **WHY AI GETS IT WRONG**: "I wake the waiter, then set the data" reads
  as natural ordering to agents, exactly the wrong order.
- **CORRECT REASONING**: `wake_up` only pokes waiters parked at that
  instant. If the condition is false at that moment, the waiter keeps
  sleeping even though the event is about to happen. Store first, wake
  second — always.
- **EXAMPLE** (bad):
  ```c
  wake_up(&wq);          /* no waiter sees a true condition */
  data_ready = true;     /* too late: race lost */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  data_ready = true;
  wake_up(&wq);
  ```
- **VERIFICATION**: harness: wake-before-store lets a parked waiter
  sleep forever (lost wakeup recorded); store-before-wake always wakes.
- **SOURCE**: kernel-completion-docs (complete/wait contract),
  kernel-source (kernel/sched/completion.c), ldd3.

## 3. Spurious wakeups are always possible

- **RULE**: a woken waiter can observe the condition still false (other
  waiters woken, timeouts, signals). The waiter must loop on the
  condition and only proceed when it is actually true.
- **WHY AI GETS IT WRONG**: agents assume "woken == event happened" and
  skip the re-check.
- **CORRECT REASONING**: `wait_event` already loops; hand-rolled wake
  handling must too. A single wakeup is a hint to re-evaluate, never a
  guarantee.
- **EXAMPLE** (bad):
  ```c
  wake_up(&wq);          /* one of N waiters woke */
  /* waiter proceeds without re-checking flag */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  wait_event(wq, flag);  /* re-checks after each wake */
  ```
- **VERIFICATION**: harness wakes a waiter before the condition is true;
  the wait_event emulator re-parks, the raw-wake pattern mis-proceeds.
- **SOURCE**: kernel-source (wait_event), kernel-completion-docs,
  ldd3.

## 4. `wake_up` vs `wake_up_interruptible` vs `wake_up_all`

- **RULE**: `wake_up` wakes all TASK_INTERRUPTIBLE and TASK_NORMAL
  waiters; `wake_up_interruptible` only TASK_INTERRUPTIBLE ones;
  `wake_up_all` wakes every waiter on the queue.
- **WHY AI GETS IT WRONG**: agents use the wrong variant for the waiter
  state, so a TASK_UNINTERRUPTIBLE waiter never wakes from
  `wake_up_interruptible`.
- **CORRECT REASONING**: the wake function must cover the state in which
  the waiter parked. `wait_event` parks in TASK_UNINTERRUPTIBLE, so its
  waker needs plain `wake_up`; interruptible waiters can be woken by
  either.
- **EXAMPLE** (bad):
  ```c
  wait_event(wq, ready);        /* TASK_UNINTERRUPTIBLE */
  ...
  wake_up_interruptible(&wq);   /* never matches: waiter never wakes */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  wait_event(wq, ready);
  ...
  wake_up(&wq);
  ```
- **VERIFICATION**: harness: mismatched wake state leaves the waiter
  parked; matched pair wakes it.
- **SOURCE**: kernel-source (kernel/sched/wait.c), kernel-driver-api.

## 5. `complete` vs `complete_all`

- **RULE**: `complete()` wakes exactly one waiter; `complete_all()` wakes
  every waiter currently waiting. After `complete_all`, `done` stays >0
  so any *future* waiter also returns immediately; reuse requires
  `reinit_completion()`.
- **WHY AI GETS IT WRONG**: agents use `complete` for many waiters or
  reuse a `complete_all`-ed completion without `reinit`, causing a
  "second event" to complete instantly.
- **CORRECT REASONING**: a completion is one-shot by design. If N
  threads must be released once, use `complete_all`; if the completion
  is reused, `reinit_completion` before arming again.
- **EXAMPLE** (bad):
  ```c
  complete_all(&done);          /* wakes all */
  ...
  complete(&done);              /* second event: no waiters left */
  /* later waiter returns immediately — stale done>0 */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  complete_all(&done);
  ...
  reinit_completion(&done);     /* arm for the next round */
  ```
- **VERIFICATION**: harness: complete wakes one; complete_all wakes all;
  reinit resets done so the next wait blocks again.
- **SOURCE**: kernel-completion-docs, kernel-source
  (kernel/sched/completion.c), ldd3.

## 6. `wait_for_completion` return values and variants

- **RULE**: `wait_for_completion` returns only when completed (or
  never). `wait_for_completion_interruptible` returns `-ERESTARTSYS` on
  signal, `wait_for_completion_killable` returns `-ERESTARTSYS` on a
  fatal signal, and the `_timeout` variants return 0 on timeout (true on
  completion). Every early return must be handled.
- **WHY AI GETS IT WRONG**: agents treat interruptible/timeout waits as
  unconditional success and proceed on an uncompleted event.
- **CORRECT REASONING**: an interrupted or timed-out wait means the
  event did NOT happen. The caller must treat the early return as an
  error/abort path, not as completion.
- **EXAMPLE** (bad):
  ```c
  wait_for_completion_interruptible(&done);  /* return ignored */
  use_shared_state();                        /* event may not have fired */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (wait_for_completion_interruptible(&done))
      return -ERESTARTSYS;                   /* signal: abort */
  use_shared_state();
  ```
- **VERIFICATION**: harness: interruptible wait returns the signal code;
  timeout wait returns 0 and the guarded state is never touched.
- **SOURCE**: kernel-completion-docs, kernel-source, ldd3.

## 7. Wait state and parking: TASK_UNINTERRUPTIBLE vs INTERRUPTIBLE

- **RULE**: `wait_event` parks in TASK_UNINTERRUPTIBLE; the interruptible
  variants park in TASK_INTERRUPTIBLE (or TASK_KILLABLE). The parked
  state is what the waker must match (see rule 4), and it decides whether
  signals can break the wait.
- **WHY AI GETS IT WRONG**: agents do not connect the parking state to
  (a) which wake function matches and (b) whether signals can abort.
- **CORRECT REASONING**: interruptible waits can be woken by signals and
  then need `-ERESTARTSYS` handling; uninterruptible waits cannot, so a
  hung uninterruptible waiter needs `wake_up_all` from somewhere.
- **EXAMPLE** (bad):
  ```c
  wait_event_interruptible(wq, cond);
  /* caller ignores that a signal may have woken it */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (wait_event_interruptible(wq, cond))
      return -ERESTARTSYS;
  ```
- **VERIFICATION**: harness: interruptible park wakes on simulated
  signal; the emulator records the interrupted path.
- **SOURCE**: kernel-source (wait_event family), kernel-driver-api.

## 8. Wake implies ordering: release/acquire across the pair

- **RULE**: data written before `wake_up`/`complete` is visible to the
  woken waiter after it is unparked (wake_up provides release semantics;
  the waiter's return from sleep provides acquire). This holds only
  across the wake/wait pair, not for unrelated observers.
- **WHY AI GETS IT WRONG**: agents either forget the ordering entirely
  or treat `wake_up` as a full barrier for everything.
- **CORRECT REASONING**: publish the data, then `wake_up`. The waiter
  that observes the wake is guaranteed to see the published data. That
  is the documented acquire/release pairing of waitqueue wakeups.
- **EXAMPLE** (bad):
  ```c
  wake_up(&wq);
  shared_value = 42;    /* waiter may read stale 0 */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  shared_value = 42;    /* published before wake */
  wake_up(&wq);
  ```
- **VERIFICATION**: harness orders the store and wake and asserts the
  waiter sees the published value.
- **SOURCE**: linux-memory-barriers, kernel-source (wake_up
  implementation), kernel-rcu-memory-barriers.

## 9. No waiting in atomic/sleep-forbidden context

- **RULE**: `wait_event`, `wait_for_completion`, and friends sleep.
  They are forbidden in hard IRQ, softirq, and while holding a spinlock
  or with preemption/preemption-off around the call in contexts that
  cannot sleep.
- **WHY AI GETS IT WRONG**: agents copy wait logic into bottom halves or
  spinlock-held paths because "it is just a check".
- **CORRECT REASONING**: sleeping while in atomic context triggers the
  scheduler's "BUG: sleeping function called from invalid context" and
  can deadlock the machine. Wakeups are allowed there; waits are not.
- **EXAMPLE** (bad):
  ```c
  spin_lock(&lock);
  wait_for_completion(&done);   /* BUG: sleeping in atomic context */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  spin_unlock(&lock);
  if (wait_for_completion_interruptible(&done))
      return -ERESTARTSYS;
  ```
- **VERIFICATION**: harness: the emulator records a "sleep in atomic
  context" violation when a wait is issued while the lock flag is held.
- **SOURCE**: kernel-source, kernel-lockdep-docs, ldd3.

## 10. Completion lifetime and `reinit_completion`

- **RULE**: a completion (or waitqueue) must outlive every waiter that
  can wait on it. Freeing a completion while a waiter is parked is a
  use-after-free. Reuse requires `reinit_completion` to reset `done`.
- **WHY AI GETS IT WRONG**: agents free driver state (containing the
  completion) at the end of `release` while a thread may still be
  blocked in `wait_for_completion`.
- **CORRECT REASONING**: teardown must first guarantee no waiters remain
  (complete/wake them), then free. Never free a struct a waiter still
  references.
- **EXAMPLE** (bad):
  ```c
  kfree(drv);   /* drv->done still has a parked waiter */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  complete_all(&drv->done);
  /* wait for waiter threads to exit (kthread_stop etc.) */
  kfree(drv);
  ```
- **VERIFICATION**: harness: freeing a completion with a parked waiter
  is flagged as an emulated use-after-free; complete-first + drain is
  clean.
- **SOURCE**: ldd3, kernel-completion-docs, kernel-source.

## Quick detection table

| Pattern | Class | Check |
|---|---|---|
| `while (!flag);` busy-wait | lost wakeup / CPU burn | replace with `wait_event` |
| condition stored after `wake_up` | lost wakeup | reorder: store, then wake |
| single wake treated as done | spurious wake | loop on condition |
| `wake_up_interruptible` for TASK_UNINTERRUPTIBLE | no wake | match wake to park state |
| `complete` for N waiters | missed wake | use `complete_all` |
| `complete_all` reuse w/o `reinit` | stale done | `reinit_completion` |
| ignored `_interruptible` return | proceed uncompleted | handle `-ERESTARTSYS` |
| `wait_for_completion` in spinlock/IRQ | sleep-in-atomic | move wait out, or use wake |
| free completion with parked waiter | UAF | complete + drain first |
