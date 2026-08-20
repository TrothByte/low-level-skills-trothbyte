---
name: waitqueue-completion-synchronization
description: Use when writing, reviewing, or debugging Linux kernel wait queues and completions — wait_event/wait_event_interruptible, wake_up/wake_up_interruptible, complete/complete_all, lost-wakeup races, spurious wakeups, and the memory-ordering rules around wakeup. Teaches how to synchronize kernel threads and drivers without lost wakeups.
---

# Waitqueues & Completion Synchronization

Rules for blocking kernel paths that wait on events: the wait_event
family, wake_up variants, and completions. Load `references/README.md`
before writing a wait/wake or complete/wait_for_completion path.

## When to use

- Writing or reviewing code that blocks until a condition becomes true:
  `wait_event`, `wait_event_interruptible`, `wait_event_timeout`,
  `wait_event_killable`.
- Waking blocked waiters with `wake_up`, `wake_up_interruptible`,
  `wake_up_all`, `wake_up_poll`, `__wake_up`.
- One-shot event signaling with `complete`, `complete_all`,
  `wait_for_completion`, `wait_for_completion_interruptible`,
  `wait_for_completion_timeout`.
- Debugging hangs, lost wakeups, or spurious-wakeup bugs in drivers,
  block layers, and protocol state machines.
- Deciding whether a waiter needs a waitqueue or a completion.

## When not to use

- Pure lock-based synchronization (mutex/spinlock) with no blocking wait.
- Code that must not sleep — atomic contexts need `spinlock` +
  `wake_up` from the other side, never `wait_event` on this side.
- User-space synchronization (futexes, condvars); the model differs.
- Replacing a correct polling loop without understanding the condition.

## What the agent often gets wrong

- Writing a bare `while (!cond);` busy-wait instead of `wait_event`.
- Setting the condition AFTER `wake_up` (or after `complete`), causing
  the classic lost wakeup: the waker checks, finds the condition false,
  and sleeps forever while the waiter already checked and went to sleep.
- Forgetting that `wait_event` re-checks the condition after every wake:
  a spurious wakeup (or a wake from an unrelated waiter) must not make
  the waiter proceed.
- Using `complete` where multiple waiters need waking (`complete_all`),
  or reusing a completion without `reinit_completion`.
- Ignoring the return of `wait_event_interruptible`/`killable`/`timeout`
  variants, which can return early with `-ERESTARTSYS`/`-EINTR`/`0`.
- Believing `wake_up` is a memory-ordering full barrier by itself; it
  has acquire/release semantics for the waker/waiter pair but does not
  make non-atomic writes magically ordered for unrelated observers.
- Calling `wait_for_completion` in a context that cannot sleep.

## How to reason correctly

1. A waitqueue pairs a parked waiter list with an event. The *condition*
   is the event; the waitqueue is only the parking mechanism. The
   condition is evaluated before parking and re-evaluated after every
   wake (this is what makes `wait_event` race-free).
2. Never set the condition after the wake call. Ordering that cannot
   lose the event:
   ```
   // waker:                        // waiter:
   cond = true;                     wait_event(wq, cond);
   wake_up(&wq);
   ```
   `wake_up` after the store provides the release; the waiter's
   re-check provides the acquire. Setting `cond` after `wake_up`
   reopens the race.
3. Spurious wakeups are guaranteed possible. The waiter must always
   loop on the condition, never trust "I was woken, therefore done".
4. `complete` wakes exactly one waiter; `complete_all` wakes all current
   waiters. After `complete_all`, later waiters see done>0 and return
   immediately — reuse requires `reinit_completion`.
5. `wait_for_completion` sleeps indefinitely; the interruptible/killable
   variants return early on signal, and the timeout variants return
   early when the timeout expires. Check every early-return value.
6. `wake_up`/`complete` from a context where the waiter can run
   immediately is fine; the parking list and done counter are
   synchronized internally. The data the waiter reads must be
   published before the wake (release/acquire pairing).

## What to verify

- Every `wait_event*` site re-checks its condition in a loop (macro does
  this) and the waker sets the condition strictly before `wake_up`.
- Every interruptible/killable/timeout wait checks its return value.
- Every completion is one-shot or `reinit_completion`-protected when
  reused; `complete_all` is not used where a single waiter is expected.
- No `wait_event`/`wait_for_completion` in atomic/sleep-forbidden
  context.
- The data read by a woken waiter was written before the wake (with the
  matching barrier semantics of `wake_up`).

## How to verify

Host-compilable logic checks (self-contained stubs, no kernel headers):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_waitqueue.c -o /tmp/good_waitqueue
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_waitqueue.c -o /tmp/bad_waitqueue
```

Target (kernel) checks — document these, do not claim to have run them:

```
# lockdep: CONFIG_LOCKDEP build; sleeping-in-atomic is reported as
# "BUG: sleeping function called from invalid context"
# KASAN VM + KUnit: waitqueue/complete kselftests under QEMU
```

## Where the knowledge comes from

- `kernel-completion-docs` — Documentation/scheduler/completion.rst:
  the completion API contract, one-shot vs `complete_all`, `reinit`
- `kernel-driver-api` — driver API docs: waitqueue usage in drivers,
  wake_up conventions
- `kernel-source` — kernel/wait.c, kernel/sched/completion.c:
  implementations, the parking-list and done-counter models
- `ldd3` — blocking I/O chapter: waitqueues, `wait_event`, wakeups,
  and the sleep/wake discipline for drivers
- `linux-memory-barriers` — wake_up/wait ordering and the acquire/
  release pairing across the waker/waiter boundary
- `kernel-lockdep-docs` — lockdep checks that surface wait-in-atomic
  and incomplete-condition bugs

## Related skills

- `kernel-atomic-context` — require: where waits may not sleep
- `kthread-create-and-teardown` — recommend: threads waiting on events
- `workqueue-flush-and-cancellation` — recommend: flush vs wait events
- `kernel-rcu-memory-barriers` — recommend: publish/wake ordering
- `deadlock-kernel-prevention` — recommend: wait/wake deadlock classes
- `memory-ordering-reasoning` — recommend: acquire/release semantics

## Evaluation

Lost-wakeup and completion bugs generally have no public CVE; they are
documented bug classes fixed per subsystem by commit. Classes:

1. Lost wakeup — the waker sets the condition after `wake_up` (or the
   waiter races the check), so a waiter parks forever. Documented in
   `kernel-completion-docs` (the complete/wait contract) and prevented
   by the `wait_event` re-check loop.
2. `complete_all` misuse — waking more waiters than a one-shot
   completion allows, or reusing a completion without `reinit_completion`,
   so a stale done>0 makes later waits return immediately.
3. Wait in atomic context — `wait_for_completion`/`wait_event` in a
   spinlock/BH/IRQ path; surfaced by lockdep and by the scheduler
   "sleeping function called from invalid context" check.

No CVE numbers are assigned or claimed for these classes. Synthetic
evals: correct condition-before-wake and one-shot completion must not be
flagged; lost-wakeup ordering, bare `while (!cond);`, ignored
interruptible returns, and `complete_all` reuse must be flagged.
Adversarial: a "correct-looking" wake that happens before the condition
store; an interruptible wait whose `-ERESTARTSYS` is treated as success.
False-positive: a waitqueue where the condition is genuinely set before
wake, and a properly `reinit`-protected completion, must NOT be flagged.
