---
name: kernel-timers-hrtimer-vs-legacy
description: Use when writing, reviewing, or debugging Linux kernel timer code — legacy timer_list (mod_timer/del_timer/del_timer_sync) vs high-resolution hrtimer (hrtimer_start/hrtimer_cancel/hrtimer_active), callback context (softirq vs tasklet/BH vs process), restarting timers in callbacks, and module unload. Teaches which timer API fits which context and the races each one hides.
---

# Kernel Timers: hrtimer vs legacy timer_list

Two timer APIs, two callback contexts, one shared trap: a callback that
runs after the data it touches is gone. Load `references/README.md` before
writing any `mod_timer` / `del_timer_sync` / `hrtimer_start` /
`hrtimer_cancel` path.

## When to use

- Writing or reviewing any kernel timer: retries, timeouts, periodic polls,
  rate limiting, hardware watchdog re-arms, delayed deactivation.
- Choosing between `timer_list` (jiffies/HZ) and `hrtimer` (nanoseconds).
- Determining what context a timer callback runs in and what is legal there.
- Restarting a periodic timer from inside its own callback.
- Tearing timers down at module unload or device close/release.

## When not to use

- User-space timers (`timerfd`, `alarm`, `setitimer`) — different model.
- Work that must sleep or block: defer to a workqueue or kthread.
- FreeRTOS/Zephyr software timers — different context rules.
- Locking/ordering questions with no timing component.

## What the agent often gets wrong

- "Timer callbacks run in process context." `timer_list` callbacks run in
  softirq context with bottom halves disabled; `hrtimer` callbacks run in
  hardirq context by default. Neither can sleep.
- "del_timer() tells me my callback will never run again." It only reports
  whether the timer *was pending* when called, and it does NOT wait for a
  callback that is already running or queued on another CPU.
- "A timer that already fired is safe to free." A fired timer's callback
  can still be queued; del_timer() then returns 0 and the callback runs on
  freed memory.
- "mod_timer() is set-and-forget." It re-arms and returns whether the timer
  was active before; freeing right after re-arm/delete without
  `del_timer_sync()` is racy.
- "hrtimer_start() from the callback AND return HRTIMER_RESTART." Pick one
  restart mechanism; doing both re-arms the timer twice.
- "hrtimer_cancel() from inside the callback is fine." It waits for the
  running callback — itself — so it deadlocks; use `hrtimer_try_to_cancel()`
  or return `HRTIMER_RESTART`.
- "The module-unload race is theoretical." Missing `del_timer_sync()` /
  `hrtimer_cancel()` before freeing the callback data is a real
  use-after-free class, not a style issue.

## How to reason correctly

1. Classify granularity first: HZ ticks are enough → `timer_list` (cheap,
   softirq context); sub-jiffy or exact short delays → `hrtimer`.
2. Know the callback context before writing the body: `timer_list` →
   softirq (atomic); `hrtimer` → hardirq (atomic), or softirq with
   `HRTIMER_MODE_*_SOFT`. No sleeping in any of them; if the work must
   sleep, defer it to a workqueue/kthread.
3. Read timer returns as facts: `del_timer()` / `del_timer_sync()` return 1
   if the timer was pending before the call, 0 if not; `mod_timer()` returns
   1 if it was active before re-arming. A 0 from `del_timer()` means "not
   pending" — not "callback done".
4. Assume "the callback may run at any instant" until `del_timer_sync()` /
   `hrtimer_cancel()` has returned. Any data the callback touches must live
   at least that long.
5. On unload/close: stop arming first, then `del_timer_sync()` /
   `hrtimer_cancel()`, then free the callback data. Never free the struct
   containing the timer before the timer is dead.
6. For periodic `hrtimer`s, advance the expiry (`hrtimer_forward`) in the
   callback and return `HRTIMER_RESTART`; return `HRTIMER_NORESTART` to
   stop. Do not also call `hrtimer_start()`.
7. In atomic callback context, avoid allocation; if unavoidable use
   `GFP_ATOMIC` and check the result (`kernel-atomic-context`).

## What to verify

- Every callback body is atomic-safe: no `kmalloc(GFP_KERNEL)`, no mutex,
  no `copy_to_user`, no `schedule()`, no `wait_event`.
- Every `del_timer()` is backed by a real `del_timer_sync()` before the
  callback data is freed, or is provably unreachable while a callback is
  queued/running.
- On unload, arming is stopped before cancel/sync; cancel/sync happens
  before the struct containing the timer or its data is freed.
- `hrtimer` teardown uses `hrtimer_cancel()` (or `hrtimer_try_to_cancel()`
  from within the callback) — not just a "kill flag" checked in the
  callback.
- Periodic `hrtimer`s advance `expires_ns` before returning
  `HRTIMER_RESTART`, and never both call `hrtimer_start()` and return
  `HRTIMER_RESTART`.
- `HRTIMER_MODE_*_SOFT` moves the callback to softirq, not process context.

## How to verify

Host-compilable logic checks (self-contained stubs, no kernel headers):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_timers.c -o /tmp/good_timers
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_timers.c -o /tmp/bad_timers
```

Target (kernel) checks — document these, do not claim to have run them:

```
# lockdep + atomic-sleep debug build, then boot and rmmod the driver;
# dmesg reports:
#   "BUG: sleeping function called from invalid context"
#   "WARNING: CPU: .. del_timer_sync() called from" (illegal context)
# and KASAN reports the use-after-free if teardown ordering is wrong.
cat /proc/timer_list        # active hrtimers, their expiry and resolution
```

## Where the knowledge comes from

- `kernel-timers-docs` — Documentation/timers/: hrtimer and timer_list
  API, callback context, and mode (ABS/REL, HARD/SOFT) semantics
- `kernel-driver-api` — driver API docs on using timers/hrtimers in
  device drivers
- `kernel-source` — kernel/time/timer.c and kernel/time/hrtimer.c contract
  comments: del_timer/del_timer_sync/mod_timer/hrtimer_cancel return values
- `ldd3` — ch.7 deferred work: timer_list usage, del_timer_sync for SMP,
  and the module-unload race when a callback outlives the driver

## Related skills

- `workqueue-flush-and-cancellation` (recommend) — process-context deferral
  for work that must sleep; flush/cancel discipline parallels timer teardown
- `kernel-atomic-context` (require) — what is legal inside a timer callback
- `kernel-driver-char-device-lifecycle` (recommend) — unload/close ordering
  where timers are torn down
- `kthread-create-and-teardown` (recommend) — process-context alternative to
  timers for periodic work that must block
- `interrupt-controller-gic-apic` (recommend) — hardirq flow surrounding
  hrtimer callbacks
- `embedded-interrupt-and-nested` (recommend) — ISR context discipline in
  the embedded/RTOS analog of this problem

## Evaluation

Timer bugs rarely get a public CVE; the honest framing is documented kernel
bug classes: missing `del_timer_sync()` before freeing timer data →
use-after-free; `del_timer()` without sync while the callback still runs on
another CPU; assuming a legacy `timer_list` callback may sleep; freeing
hrtimer data without `hrtimer_cancel()`; `hrtimer_cancel()` from inside its
own callback → self-deadlock. Synthetic one-liners: `mod_timer()` on freed
memory, `del_timer()` on a re-armed timer, `hrtimer_start()` without cancel
at unload, `GFP_KERNEL` kmalloc in a callback. Adversarial: a "working"
single-CPU test that races on SMP; a periodic hrtimer whose callback forgets
to advance `expires_ns` and re-fires in a busy loop. False-positive: correct
`del_timer_sync()`/`hrtimer_cancel()` before free, periodic restart via
`HRTIMER_RESTART`, `mod_timer()` re-arm under the arming lock, softirq-only
`_SOFT` callbacks — must NOT be flagged.
