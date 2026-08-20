# Evaluation — kernel-timers-hrtimer-vs-legacy

Skill: `skills/kernel/kernel-timers-hrtimer-vs-legacy`. Stability target:
`evaluated`.

## Verified facts (host, this run)

Host: Windows, MinGW gcc 16.1.0, Python 3.11.9. Self-contained stubs
(`examples/stubs.h`), no kernel headers required.

- Good example compiles clean with `gcc -Wall -Wextra -Werror -O2`,
  runs with all assertions passing (exit 0).
- Bad example compiles with the same flags and reproduces both bug
  classes as runtime diagnostics (exit 0) without crashing the harness.
- SKILL.md passes `tools/lint/skill_lint.py` with 0 errors / 0 warnings.
- SKILL.md activation cost measured with `tools/tokens/token_measure.py
  --check 2000`: 1770 tokens (metadata 87 + body 1683) — under the gate.

| Example | Compile | Run | Observation |
|---|---|---|---|
| good/good_timers.c | 0 | 0 | prints "ALL CHECKS PASSED"; asserts mod_timer returns, fire-on-tick, del_timer vs del_timer_sync, ABS/REL modes, start-replaces, HRTIMER_RESTART self-restart, cancel-before-free, hardirq/softirq context flags |
| bad/bad_timers.c | 0 | 0 | prints "BUG reproduced: timer fired after data freed (del_timer without sync)" and "BUG reproduced: hrtimer callback ran after data freed (no hrtimer_cancel)" |

NOT verified on this host (documented targets, do NOT claim to have run):
real kernel build, lockdep/DEBUG_ATOMIC_SLEEP, KASAN VM, QEMU,
`/proc/timer_list` inspection.

## Historical bug-class evals

Timer bugs often have no public CVE. The honest framing is the documented
kernel bug classes below; no CVE numbers are invented or implied.

| Bug class | Where it lives | Detect | Fix | Verify |
|---|---|---|---|---|
| missing `del_timer_sync()` before freeing timer data | any driver rmmod/close path; kernel tree is full of "use del_timer_sync() before free" fixes | free of a `struct` that contains (or is referenced by) a live `timer_list` | stop arming under the arming lock, then `del_timer_sync()`, then free | KASAN VM + rmmod loop |
| `del_timer()` (not sync) while callback still running/queued | SMP teardown paths; single-CPU tests never show it | `del_timer()` with no subsequent sync before `kfree` | replace with `del_timer_sync()`; never free the callback's data on a 0 return alone | SMP VM + stress |
| sleeping inside a legacy timer callback | `GFP_KERNEL` / mutex / `copy_to_user` in a `timer_list` callback | "BUG: sleeping function called from invalid context" dmesg | move work to workqueue/kthread; keep callback atomic | DEBUG_ATOMIC_SLEEP boot |
| freeing hrtimer data without `hrtimer_cancel()` | unload/close with a queued or running hrtimer | `hrtimer_active()` or "will never fire" assumption used instead of cancel | `hrtimer_cancel()` (or `hrtimer_try_to_cancel()` in-callback) before free | KASAN VM + rmmod loop |
| `hrtimer_cancel()` from inside the hrtimer's own callback | periodic timers stopping themselves | hang / soft lockup | return `HRTIMER_NORESTART` (optionally after a `stop` flag) | lockdep + VM |
| double-arm: `hrtimer_start()` in callback plus `HRTIMER_RESTART` | periodic timers | fires twice per period or busy re-fire | pick one restart mechanism; advance `expires_ns` before `HRTIMER_RESTART` | synthetic harness |

## Synthetic evals

- easy/positive: `del_timer_sync()` + `hrtimer_cancel()` before free must
  NOT be flagged.
- easy/negative: `del_timer()` then `kfree` of the timer's data must be
  flagged as use-after-free.
- medium/negative: sleep (`GFP_KERNEL` kmalloc, mutex) inside a timer
  callback must be flagged.
- medium/negative: free after `del_timer()` returning 0 (expired case)
  must be flagged.
- hard/negative: `hrtimer_cancel()` called from its own callback must be
  flagged as self-deadlock.
- hard/negative: `HRTIMER_RESTART` without advancing `expires_ns` (busy
  re-fire) must be flagged.

## Adversarial evals

- Code that "passes" a single-CPU smoke test because the expired-callback
  race never shows up without SMP timing — agent must not declare it
  correct.
- A periodic hrtimer that re-arms via `hrtimer_start()` in the callback
  and returns `HRTIMER_RESTART`; must be detected as double-arm.
- A `timer_list` callback that "works" until BH disabled forces no sleep;
  must be detected as atomic-context violation.
- Unload code that frees the device struct, then calls
  `hrtimer_cancel()`/`del_timer_sync()` after — wrong order, must be
  flagged.

## False-positive evals (correct code must not be flagged)

- `del_timer_sync()` / `hrtimer_cancel()` before free — do NOT flag.
- Periodic restart via `HRTIMER_RESTART` with `expires_ns` advanced
  (`hrtimer_forward`) — do NOT flag.
- `mod_timer()` re-arm under the same lock that guards the timer data —
  do NOT flag.
- `_SOFT` hrtimer callbacks used purely to escape hardirq context — do NOT
  flag (they still may not sleep).
- `GFP_ATOMIC` allocation with a NULL check inside a callback — do NOT
  flag.

## Verification commands

Host (self-contained stubs — recorded this run):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_timers.c -o /tmp/good_timers
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_timers.c -o /tmp/bad_timers
# run: good_timers -> "ALL CHECKS PASSED", exit 0
# run: bad_timers -> two "BUG reproduced: ..." lines, exit 0
```

Static gate (recorded this run):

```
python tools/lint/skill_lint.py skills/kernel/kernel-timers-hrtimer-vs-legacy/SKILL.md
python tools/tokens/token_measure.py --check 2000 skills/kernel/kernel-timers-hrtimer-vs-legacy
```

Target (kernel) — documented only, NOT run here:

```
# lockdep + atomic-sleep debugging
make defconfig
scripts/config -e PROVE_LOCKING -e DEBUG_ATOMIC_SLEEP -e KASAN
make -j$(nproc)
qemu-system-x86_64 -kernel arch/x86/boot/bzImage \
  -append "console=ttyS0 kasan=on" -nographic
# drive the driver's timer path, then rmmod in a loop; dmesg must show
# no "BUG: sleeping function called from invalid context" and no KASAN
# use-after-free report.

# inspect live high-resolution timers
cat /proc/timer_list

# kernel-side timer unit tests
./tools/testing/selftests/...  # or KUnit timer/hrtimer suites if built
```

## Scoring

- precision: every flagged pattern maps to a real timer/context rule.
- recall: each bad snippet is detected.
- FP-rate: good snippets produce zero flags.
- calibration: return-value and context claims (rules 1-12 in
  `references/README.md`) are marked KNOWN where documented; nothing is
  flagged with a confidence higher than its source supports.
