# Linux Kernel Timer Rules: hrtimer vs legacy timer_list

Source-backed rule set for choosing and using the two Linux kernel timer
APIs. Each entry: RULE -> WHY AI GETS IT WRONG -> CORRECT REASONING ->
EXAMPLE -> COUNTEREXAMPLE -> VERIFICATION -> SOURCE. Confidence markers:
KNOWN (documented contract), INFERRED (derived), UNVERIFIED (never use in a
stable skill).

## 1. `timer_list` expires in jiffies, not milliseconds

- **RULE**: a `struct timer_list` timer armed with `mod_timer()` expires in
  jiffies — whole HZ ticks. Resolution is at best 1/HZ seconds (1 ms at
  1000 Hz, 4 ms at 250 Hz, 10 ms at 100 Hz). Convert wall-time delays with
  `msecs_to_jiffies()`, `usecs_to_jiffies()`, or `HZ`.
- **WHY AI GETS IT WRONG**: agents pass milliseconds straight into
  `mod_timer()` or treat the expiry as sub-tick, then wonder why the
  callback is late.
- **CORRECT REASONING**: `mod_timer(timer, expires)` takes an ABSOLUTE
  jiffies value. A delay from now is `jiffies + msecs_to_jiffies(ms)`.
  If the required granularity is finer than a tick — or the timeout is
  short and must be precise — `timer_list` is the wrong tool; use an
  `hrtimer` (rule 6).
- **EXAMPLE** (bad):
  ```c
  mod_timer_emu(&t, 100);          /* "100 ms" — actually 100 ticks */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  mod_timer_emu(&t, msecs_to_jiffies_emu(100));   /* HZ-dependent */
  ```
- **VERIFICATION**: harness: a 1-tick timer fires on the next
  `timer_tick_emu()`; a 100 ms timer only fires after `HZ/10` ticks.
- **SOURCE**: kernel-timers-docs (timer API); ldd3 (ch.7, "Kernel
  Timers").

## 2. `timer_list` callbacks run in softirq context; they cannot sleep

- **RULE**: when a `timer_list` timer expires, its callback runs in
  softirq context (`TIMER_SOFTIRQ`) with bottom halves disabled. It is
  atomic context: no sleeping, no `GFP_KERNEL` allocation, no mutex, no
  `copy_to_user`, no `schedule()`.
- **WHY AI GETS IT WRONG**: agents pattern-match on the word "timer" and
  write process-context code (mutexes, `GFP_KERNEL` kmalloc) inside the
  callback, which splats at runtime.
- **CORRECT REASONING**: the timer softirq runs between
  `local_bh_disable()`/`local_bh_enable()`, so the callback must behave
  like any other bottom-half code. If the deferred work needs to sleep,
  the callback must re-defer it to a workqueue or kthread.
- **EXAMPLE** (bad):
  ```c
  void my_timer_cb(struct timer_list *t) { kmalloc(GFP_KERNEL, ...); }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  void my_timer_cb(struct timer_list *t) { schedule_work(&dev->work); }
  ```
- **VERIFICATION**: harness asserts `legacy_in_softirq_ctx == 1` inside
  the callback and `context_forbids_sleep_emu() == 1`.
- **SOURCE**: kernel-timers-docs (timer API, context requirements);
  kernel-source (kernel/time/timer.c softirq dispatch); ldd3 (ch.7).

## 3. `mod_timer()` semantics: re-arm + return value

- **RULE**: `mod_timer(timer, expires)` re-arms the timer at the new
  absolute jiffies expiry and returns 1 if the timer was active (pending)
  before the call, 0 if it was inactive. Calling it on an active timer is
  safe and simply moves the expiry. It may be called from interrupt and
  softirq context.
- **WHY AI GETS IT WRONG**: agents treat `mod_timer()` as fire-and-forget
  and ignore the return, or free the timer's data believing a re-arm is
  a cancellation.
- **CORRECT REASONING**: the return tells you whether the timer was
  already queued — useful for refcounting/state transitions — but it says
  nothing about a callback that is currently running. `mod_timer()` does
  not wait; only `del_timer_sync()` guarantees a callback is done.
- **EXAMPLE** (bad):
  ```c
  mod_timer_emu(&dev->tmr, 5);        /* return ignored */
  /* ... later, same tick: */
  free(dev);                          /* callback may still run on dev */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  if (mod_timer_emu(&dev->tmr, 5) == 0)
      get_ref(dev);                   /* was inactive: now it is armed */
  ```
- **VERIFICATION**: harness: first `mod_timer_emu` on an idle timer
  returns 0; the second on the armed timer returns 1; the timer fires at
  the new expiry only.
- **SOURCE**: kernel-source (kernel/time/timer.c `__mod_timer()`
  contract); kernel-timers-docs.

## 4. `del_timer()`: removes a pending timer, does NOT wait

- **RULE**: `del_timer(timer)` dequeues a pending timer and returns 1 if
  the timer was pending before the call, 0 if it was not. It does NOT
  wait for a callback that is already running on another CPU or already
  dequeued-and-queued-to-run.
- **WHY AI GETS IT WRONG**: agents read `del_timer()` as "callback is now
  guaranteed not to run", which is only true for the not-yet-fired case
  on a single CPU.
- **CORRECT REASONING**: "pending" means "queued in the timer list".
  Once the timer has expired, it is removed from the list before the
  callback runs; `del_timer()` then returns 0 even though the callback is
  still in flight. A return of 0 from `del_timer()` is exactly the
  dangerous case: the callback may still touch the data.
- **EXAMPLE** (bad):
  ```c
  del_timer_emu(&dev->tmr);   /* returns 0: already expired */
  free(dev);                  /* callback still queued -> UAF */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  del_timer_sync_emu(&dev->tmr);   /* waits for any in-flight callback */
  free(dev);
  ```
- **VERIFICATION**: harness: with `legacy_defer_fire = 1` the expired
  callback stays queued; `del_timer_emu` returns 0 and the orphan fires
  on the next tick, `del_timer_sync_emu` runs it before returning.
- **SOURCE**: kernel-source (kernel/time/timer.c); kernel-timers-docs;
  ldd3 (ch.7, "del_timer" vs "del_timer_sync").

## 5. `del_timer_sync()` and why it must precede free

- **RULE**: `del_timer_sync(timer)` dequeues the timer AND blocks until
  any concurrently running callback has completed. It must be used before
  freeing the memory the callback accesses. It may sleep, so it must NOT
  be called from interrupt context, from a softirq, or while holding a
  spinlock that the callback takes (self-deadlock).
- **WHY AI GETS IT WRONG**: agents use `del_timer()` everywhere because
  it "works" in a single-CPU test, then the module oopses under load at
  rmmod or when the device is released.
- **CORRECT REASONING**: the callback can be running on another CPU the
  instant `del_timer()` returns. `del_timer_sync()` closes that window by
  waiting. The contract is: stop arming (under the arming lock), then
  `del_timer_sync()`, then free — in that order. On SMP the plain
  `del_timer()` is only safe if the data provably outlives any possible
  callback, which for heap/freed data it never does.
- **EXAMPLE** (bad):
  ```c
  del_timer_emu(&dev->tmr);
  kfree_emu(dev);        /* callback may still be running on CPU1 */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  spin_lock(&dev->lock);          /* stop arming under the arming lock */
  dev->stop = 1;
  spin_unlock(&dev->lock);
  del_timer_sync_emu(&dev->tmr);  /* waits for the callback */
  kfree_emu(dev);
  ```
- **VERIFICATION**: harness: the deferred-callback scenario only runs the
  callback before return when `del_timer_sync_emu` is used.
- **SOURCE**: kernel-timers-docs (timer API); kernel-source
  (kernel/time/timer.c); ldd3 (ch.7, "The module unload problem").

## 6. `hrtimer`: nanosecond resolution, ABS/REL and HARD/SOFT modes

- **RULE**: `hrtimer`s are high-resolution timers on a
  `ktime_t`/nanosecond clock (`CLOCK_MONOTONIC`). `HRTIMER_MODE_ABS`
  means the given expiry is absolute; `HRTIMER_MODE_REL` means the expiry
  is relative to now and is converted to absolute at arming time.
  `HRTIMER_MODE_ABS_SOFT` / `HRTIMER_MODE_REL_SOFT` additionally run the
  callback in softirq context instead of hardirq context.
- **WHY AI GETS IT WRONG**: agents mix REL/ABS (a "REL" value reused after
  the clock advanced fires late), or expect `_SOFT` to make the callback
  sleepable.
- **CORRECT REASONING**: choose ABS when the expiry is an absolute point
  in time and REL for "in N ns from now". REL is evaluated against
  "now" at arming; a long REL expiry is generally better expressed as
  ABS against a known monotonic time. The HARD/SOFT bit changes the
  callback context (rule 7), never the ability to sleep.
- **EXAMPLE** (bad):
  ```c
  hrtimer_start_emu(&t, 1000, HRTIMER_MODE_REL, cb, d);
  /* 10 ms later the driver reuses "1000" as if it were absolute */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  hrtimer_start_emu(&t, now_ns + 1000, HRTIMER_MODE_ABS, cb, d);
  ```
- **VERIFICATION**: harness: a REL timer armed with 500 ns fires at
  now+500; an ABS timer fires at exactly its expiry.
- **SOURCE**: kernel-timers-docs (hrtimer API and modes); kernel-source
  (kernel/time/hrtimer.c mode handling).

## 7. `hrtimer` callbacks run in hardirq context by default — no sleeping

- **RULE**: a default-mode (`HRTIMER_MODE_ABS`/`HRTIMER_MODE_REL`) hrtimer
  callback executes in hardirq (interrupt) context: no sleeping, no
  `GFP_KERNEL`, no mutex, no blocking I/O. The `_SOFT` variants execute it
  in softirq context — still no sleeping.
- **WHY AI GETS IT WRONG**: agents read "timer" as "background thread",
  write sleeping code in the callback, and hit "BUG: sleeping function
  called from invalid context" or, worse, a live lock.
- **CORRECT REASONING**: hardirq context preempts everything; the callback
  must be non-blocking and short. Allocating uses `GFP_ATOMIC` (and the
  result must be checked); deferring is done by returning
  `HRTIMER_RESTART`/`HRTIMER_NORESTART` or by handing work to a
  workqueue/kthread.
- **EXAMPLE** (bad):
  ```c
  static enum hrtimer_restart cb(struct hrtimer *t) {
      mutex_lock(&dev->mtx);        /* sleeps in hardirq context */
      return HRTIMER_NORESTART;
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static enum hrtimer_restart cb(struct hrtimer *t) {
      if (!queue_work(dev->wq, &dev->work))
          return HRTIMER_RESTART;   /* re-arm if deferral was missed */
      return HRTIMER_NORESTART;
  }
  ```
- **VERIFICATION**: harness asserts `hrtimer_in_hardirq_ctx == 1` for the
  default mode, `0` for `_SOFT`, and `context_forbids_sleep_emu() == 1`
  in both.
- **SOURCE**: kernel-timers-docs (hrtimer context requirements);
  kernel-driver-api (timers/hrtimers in drivers).

## 8. `hrtimer_start()` replaces a pending expiry

- **RULE**: `hrtimer_start(timer, expires, mode)` arms the timer, or if it
  is already active, re-arms it at the new expiry — the old pending expiry
  is replaced. It is safe to call on an active timer.
- **WHY AI GETS IT WRONG**: agents guard the start with "is it active?"
  checks as if a second start were illegal, or assume a started timer is
  single-shot and stop relying on the replace behavior.
- **CORRECT REASONING**: starting an active timer just moves its expiry;
  the callback that was armed at the old time will not fire. "Was it
  active before" is obtained from `hrtimer_is_queued()`/`hrtimer_active()`
  (rule 10), not from the start call itself.
- **EXAMPLE** (bad):
  ```c
  if (hrtimer_active_emu(&t)) return -EBUSY;   /* needless rejection */
  hrtimer_start_emu(&t, 1000, HRTIMER_MODE_ABS, cb, d);
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  hrtimer_start_emu(&t, 2000, HRTIMER_MODE_ABS, cb, d);
  /* replaces any pending 1000 ns expiry */
  ```
- **VERIFICATION**: harness: start at 1000, re-start at 3000, advance to
  1500 — nothing fires; at 3000 it fires once.
- **SOURCE**: kernel-timers-docs (hrtimer API); kernel-source
  (kernel/time/hrtimer.c `hrtimer_start()`).

## 9. `hrtimer_cancel()` waits for the running callback

- **RULE**: `hrtimer_cancel(timer)` cancels the timer and, if its callback
  is executing, waits for it to finish. It returns 0 if the timer was
  inactive, 1 if it was active and cancelled, -1 if the callback was
  running and had to be waited for. Because it can wait, it must not be
  called from the timer's own callback (it would wait on itself); there,
  use `hrtimer_try_to_cancel()`.
- **WHY AI GETS IT WRONG**: agents call `hrtimer_cancel()` from the
  callback to stop a periodic timer and hang the CPU, or skip it at
  unload because `hrtimer_active()` "looked false".
- **CORRECT REASONING**: `hrtimer_cancel()` is the hrtimer equivalent of
  `del_timer_sync()`: after it returns, no callback can be running. It is
  the required step before freeing the data the callback accesses. A
  running callback should stop itself by returning `HRTIMER_NORESTART`
  (possibly after a `bool stop` check), not by cancelling.
- **EXAMPLE** (bad):
  ```c
  static enum hrtimer_restart cb(struct hrtimer *t) {
      hrtimer_cancel_emu(t);        /* waits on itself: deadlock */
      return HRTIMER_NORESTART;
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static enum hrtimer_restart cb(struct hrtimer *t) {
      return dev->stop ? HRTIMER_NORESTART : HRTIMER_RESTART;
  }
  /* teardown: */
  dev->stop = 1;
  hrtimer_cancel_emu(&dev->tmr);    /* waits for any in-flight cb */
  ```
- **VERIFICATION**: harness: `hrtimer_cancel_emu` returns 1 for a queued
  timer and 0 for an inactive one; a cancelled timer never fires.
- **SOURCE**: kernel-timers-docs (hrtimer API); kernel-source
  (kernel/time/hrtimer.c `hrtimer_cancel()`).

## 10. `hrtimer_active()` / `hrtimer_is_queued()`

- **RULE**: `hrtimer_active(timer)` returns true while the timer is queued
  OR its callback is running (i.e. it is not inactive); `hrtimer_is_queued()`
  returns true only while the timer is actually on the queue.
- **WHY AI GETS IT WRONG**: agents use `hrtimer_active()` as "will fire
  again", but it is also true while the callback is executing.
- **CORRECT REASONING**: `hrtimer_active()` is the "not dead" check for
  teardown decisions; it does not mean the timer is armed for a future
  fire. `hrtimer_is_queued()` is the precise "still pending" check. Inside
  the callback, neither says "safe to free": only `hrtimer_cancel()` (or
  `del_timer_sync()` for timer_list) gives that guarantee.
- **EXAMPLE** (bad):
  ```c
  if (!hrtimer_active_emu(&t)) free(dev);   /* callback may be running */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  hrtimer_cancel_emu(&dev->tmr);
  free(dev);
  ```
- **VERIFICATION**: harness: active is 1 for queued and during the
  callback; is_queued drops to 0 once the timer fires.
- **SOURCE**: kernel-timers-docs (hrtimer API); kernel-source
  (include/linux/hrtimer.h `hrtimer_active`).

## 11. Self-restarting hrtimers: return `HRTIMER_RESTART`

- **RULE**: a periodic hrtimer is restarted by advancing the expiry (e.g.
  `hrtimer_forward()` or updating `expires_ns`) and returning
  `HRTIMER_RESTART` from the callback; returning `HRTIMER_NORESTART` stops
  it. Do NOT combine this with a `hrtimer_start()` call in the callback —
  that is the alternative mechanism and double-arming a timer is a bug.
- **WHY AI GETS IT WRONG**: agents call `hrtimer_start()` inside the
  callback to "restart" it AND return `HRTIMER_RESTART`, or return
  `HRTIMER_RESTART` without advancing the expiry, re-firing in a hot loop.
- **CORRECT REASONING**: the return-value mechanism is the kernel-idiomatic
  path: update the next expiry first, then return `HRTIMER_RESTART`. If
  you prefer `hrtimer_start()` in the callback, return `HRTIMER_NORESTART`.
  Either way, one restart per firing.
- **EXAMPLE** (bad):
  ```c
  hrtimer_start_emu(&t, next, HRTIMER_MODE_ABS, cb, d); /* re-arms */
  return HRTIMER_RESTART;      /* restarts AGAIN -> double arm */
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  t->expires_ns += period_ns;   /* or hrtimer_forward(t, now, period) */
  return HRTIMER_RESTART;
  ```
- **VERIFICATION**: harness: a periodic callback with advancing
  `expires_ns` fires exactly 3 times and then stops; a fixed-expiry
  `HRTIMER_RESTART` would fire every `hrtimer_run_emu` call.
- **SOURCE**: kernel-timers-docs (hrtimer API, restart semantics);
  kernel-source (kernel/time/hrtimer.c return handling).

## 12. Module unload / teardown ordering; free-while-pending is UAF

- **RULE**: before freeing the memory a timer callback touches (and before
  freeing the `struct` containing the timer), the timer must be proven
  dead: stop arming under the arming lock, then
  `del_timer_sync()` / `hrtimer_cancel()`, then free. Doing this in any
  other order, or skipping the sync/cancel, is a use-after-free.
- **WHY AI GETS IT WRONG**: unload/close code runs in process context and
  "works" every time in a quiet test; under load the last pending callback
  races the rmmod path, and KASAN/use-after-free reports arrive days later.
- **CORRECT REASONING**: the callback is an asynchronous agent; the unload
  path can never outrun it without an explicit synchronization point.
  `del_timer_sync()` / `hrtimer_cancel()` ARE that point. The inverse
  order (free, then cancel) frees memory the cancel path itself may touch.
- **EXAMPLE** (bad):
  ```c
  static void __exit driver_exit(void) {
      del_timer_emu(&dev->tmr);   /* returns 0: already expired */
      kfree_emu(dev);             /* queued cb runs on freed dev */
  }
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  static void __exit driver_exit(void) {
      spin_lock(&dev->lock); dev->stop = 1; spin_unlock(&dev->lock);
      del_timer_sync_emu(&dev->tmr);    /* wait: no cb can run */
      hrtimer_cancel_emu(&dev->ht);     /* same for the hrtimer */
      kfree_emu(dev);
  }
  ```
- **VERIFICATION**: harness: the deferred-callback scenario fires on freed
  data after `del_timer_emu`, never after `del_timer_sync_emu` /
  `hrtimer_cancel_emu`.
- **SOURCE**: ldd3 (ch.7, module unload problem); kernel-timers-docs;
  kernel-source (timer/hrtimer teardown patterns in drivers).

## Quick detection table

| Pattern | Class | Check |
|---|---|---|
| sleeping call in a timer callback | atomic-context bug | review callback; lockdep |
| `mod_timer()` return ignored before refcount/free | CWE-416 / 362 | read the return |
| `del_timer()` then free | use-after-free | must be `del_timer_sync()` |
| free before `del_timer_sync()`/`hrtimer_cancel()` | CWE-416 | reorder teardown |
| `hrtimer_cancel()` inside the callback | self-deadlock | use try_to_cancel / return |
| `hrtimer_start()` + `HRTIMER_RESTART` together | double arm | pick one restart path |
| `HRTIMER_RESTART` without advancing expiry | busy re-fire | `hrtimer_forward` first |
| ms passed as jiffies | late/early expiry | `msecs_to_jiffies()` |
| REL expiry reused as ABS | wrong fire time | use ABS explicitly |
| `_SOFT` expected to allow sleep | atomic-context bug | softirq still no-sleep |
