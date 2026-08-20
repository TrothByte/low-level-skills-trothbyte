# Evaluation — waitqueue-completion-synchronization

Skill: `skills/kernel/waitqueue-completion-synchronization`. Stability
target: `evaluated`.

## Verified facts (host, this run)

- All examples compile clean with `gcc -Wall -Wextra -Werror -O2` using
  self-contained stubs (`examples/stubs.h`) — no kernel headers required.
- Good examples run with assertions passing (exit 0).
- Bad examples compile and run; each reproduces its flaw and prints a
  "BUG reproduced" diagnostic (exit 0) without crashing the harness.

| Example | Compile | Run | Observation |
|---|---|---|---|
| good/good_waitqueue.c | 0 | 0 | assertions pass (condition-before-wake, park/wake, spurious re-park, complete vs complete_all, reinit, interruptible return, timeout) |
| bad/bad_waitqueue.c | 0 | 0 | "BUG reproduced: condition stored after wake point (lost wakeup)", "BUG reproduced: completion reused without reinit (stale done)", "BUG reproduced: interruptible wait returned -512 but return ignored", "BUG reproduced: wait_for_completion in atomic context", "BUG reproduced: completion freed while waiter parked (UAF)" |

NOT verified on this host (documented targets, do NOT claim to have run):
kernel build + lockdep, KASAN/KUnit waitqueue and completion tests under
QEMU, kselftests.

## Historical bug-class evals (adversarial)

| Bug class | Symptom | Detect | Fix | Verify |
|---|---|---|---|---|
| Lost wakeup (condition stored after `wake_up`/`complete`) | waiter parks forever | waker stores the condition after the wake call, or skips the wake because "no waiter is parked yet" | always store the condition before `wake_up`; rely on the `wait_event` re-check loop | harness: store-before-wake never loses the event; lockdep VM stress test |
| `complete_all` reuse without `reinit_completion` | second wait returns immediately even though the event never occurred | a completion is used for more than one round without `reinit_completion` | `reinit_completion` before re-arming | harness: reinit resets `done`; wait parks again |
| Ignored `-ERESTARTSYS` / timeout return | code proceeds on an uncompleted event | `wait_event_interruptible`/`killable`/`_timeout` return value discarded | handle every early return as an abort/error path | harness: signal causes early return; guarded state untouched |
| `wait_for_completion` in atomic context | scheduler BUG / deadlock | wait issued while holding a spinlock or in IRQ/BH context | move the wait out of atomic context, or use wakeup from the atomic side | lockdep reports "sleeping function called from invalid context" |
| Completion freed while waiter parked | use-after-free | `kfree` of driver state containing a completion while a thread is blocked in `wait_for_completion` | complete/wake all waiters and drain threads before freeing | KASAN VM + reproducer |

Each eval: DETECT (find the missed rule) -> EXPLAIN (which
waitqueue/completion rule was violated) -> FIX (apply the contract) ->
VERIFY (harness asserts the corrected behavior).

## Synthetic evals

- easy/positive: condition stored before `wake_up` — must NOT be flagged.
- easy/negative: bare `while (!cond);` busy-wait — must be flagged.
- medium/negative: `wake_up` before the condition store — must be flagged.
- medium/negative: `complete` where multiple waiters need waking — must be
  flagged (use `complete_all`).
- hard/negative: `complete_all`-ed completion reused without `reinit` —
  must be flagged.
- hard/negative: ignored `wait_event_interruptible` return — must be
  flagged.

## Adversarial evals

- A "correct-looking" waker that checks the waiter list before deciding
  whether to wake, then stores the condition later — agent must not
  declare it correct.
- An interruptible wait whose `-ERESTARTSYS` is treated as success.
- A completion where the second round "works" in a unit test because the
  first `complete_all` never ran a waiter — stale `done` hides the bug.
- `wait_for_completion` guarded by "it returns fast" but still in a
  spinlock-held path.

## False-positive evals (correct code must not be flagged)

- `wait_event` with the condition genuinely set before `wake_up` — do NOT
  flag.
- A properly `reinit_completion`-protected reusable completion — do NOT
  flag.
- `wake_up` called with no waiter parked (wake-before-wait is legal when
  the condition is already true) — do NOT flag.
- An interruptible wait whose return IS checked and aborts on signal — do
  NOT flag.
- `complete_all` used intentionally for a broadcast wake — do NOT flag.

## Verification commands

Host (self-contained stubs — recorded this run):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_waitqueue.c -o /tmp/good_waitqueue
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_waitqueue.c -o /tmp/bad_waitqueue
```

Target (kernel) — documented only, NOT run here:

```
# lockdep build catches sleep-in-atomic and wait/wake lock inversions
make defconfig && make -j$(nproc) CONFIG_LOCKDEP=y

# KASAN VM: kernel with CONFIG_KASAN booted under QEMU, completion
# kselftests + a driver exercising the wait/wake paths
qemu-system-x86_64 -kernel arch/x86/boot/bzImage \
  -append "console=ttyS0 kasan=on" -nographic

# kselftests for completion/waitqueue if built in-tree
tools/testing/selftests/run_kselftest.sh
```

## Scoring

- precision: every flagged pattern maps to a real waitqueue/completion
  rule.
- recall: each bad snippet is detected.
- FP-rate: good snippets produce zero flags.
