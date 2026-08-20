# Evaluation — kthread-create-and-teardown

Skill: `skills/kernel/kthread-create-and-teardown`. Stability target:
`evaluated`.

## Verified facts (host, this run)

- Host toolchain: MinGW gcc 16.1.0 on Windows (PowerShell), Python 3.11.
  Examples compiled with `gcc -Wall -Wextra -Werror -O2` against
  self-contained stubs (`examples/stubs.h`) — no kernel headers, no
  pthreads; the lifecycle is simulated deterministically.
- Good example runs with all assertions passing (exit 0).
- Bad example compiles and runs; each reproduced flaw prints a
  "BUG reproduced" diagnostic (exit 0) without crashing the harness.
- `tools/lint/skill_lint.py`: OK (0 warnings, 0 errors).
- `tools/tokens/token_measure.py --check 2000` (tiktoken cl100k_base):
  activation cost 1663 tokens, within the 2000-token gate.

| Example | Compile | Run | Observation |
|---|---|---|---|
| good/good_kthread.c | 0 | 0 | "ALL CHECKS PASSED"; create-not-started, drain, stop-while-alive, stop-before-free all asserted |
| bad/bad_kthread.c | 0 | 0 | "BUG reproduced: resources freed before kthread_stop"; "BUG reproduced: kthread_stop on exited task" |

NOT verified on this host (documented targets, do NOT claim to have run):
kernel build, module load/unload loop, KASAN, lockdep, real-thread timing
races under load.

## Historical bug-class evals (documented, honest framing)

No CVE numbers are invented for this skill. The classes below are
documented kthread contract violations (`kernel-kthread-docs`,
`kernel-source`); each maps to a fixture and a detect/fix/verify loop.

| Documented bug class | Fixture pattern | Detect | Fix | Verify |
|---|---|---|---|---|
| `kthread_stop` on an exited task | one-shot threadfn that returns on its own; driver still stops it later | threadfn self-exit races with the later stop (use-after-exit) | threadfn loops until `kthread_should_stop()`, or completion-prove liveness | harness: `kthread_stop_emu` on EXITED flags it |
| module unload without stopping the thread | `module_exit` frees driver state and returns; kthread still loops | thread keeps touching freed code/data after exit | `kthread_stop()` first in `module_exit`, then free | stop-before-free sentinel + load/unload loop |
| threadfn never checks `kthread_should_stop()` | `while (1)` / `for(;;)` loop with `wait_event` not including stop | `kthread_stop()` hangs waiting for a thread that will not exit | poll the flag each iteration; include it in wait conditions | harness: no-stop-poll thread cannot exit cleanly |
| waking a stopped kthread | `kthread_wake`/stop reissued after stop already ran the thread out | wake of a dead task (use-after-death) | never wake or stop after `kthread_stop` | review + harness state assertions |

Each eval: DETECT (find the contract violation) -> EXPLAIN (which kthread
rule was violated) -> FIX (reorder / poll the flag / prove liveness) ->
VERIFY (harness + documented kernel checks).

## Synthetic evals

- easy/positive: `kthread_run` + threadfn polling `kthread_should_stop` +
  stop-while-alive + free-after-stop must NOT be flagged.
- easy/negative: `kthread_stop` before freeing the thread's data must be
  flagged.
- medium/negative: `kthread_stop` on a task that already exited on its own
  must be flagged.
- medium/negative: threadfn loop with no `kthread_should_stop()` poll must
  be flagged.
- hard/negative: module exit that returns without stopping the kthread must
  be flagged.

## Adversarial evals

- Code that "passes" because the module is never unloaded in the test — the
  teardown path is never exercised; the agent must still review it.
- A thread that exits on its own (one-shot) while the driver unconditionally
  `kthread_stop`s it later — the stop looks harmless in a single-threaded
  demo.
- `kthread_stop` called from a timer/atomic context where sleeping is
  illegal — compiles fine, breaks only at runtime under lockdep.
- A recreated thread after `kthread_stop` used as "pause/resume" instead of
  `kthread_park`/`kthread_unpark`.

## False-positive evals (correct code must not be flagged)

- `kthread_stop` after a completion handshake proved the thread started
  (alive) — do NOT flag.
- Resources freed only after `kthread_stop` returns — do NOT flag.
- `kthread_stop` on a created-but-never-woken task (valid: stop runs it
  out) — do NOT flag.
- Workqueue usage (`queue_work` + `cancel_work_sync`) instead of a kthread
  — do NOT flag.

## Verification commands

Host (self-contained stubs — recorded this run; outputs to the session
temp dir):

```
gcc -Wall -Wextra -Werror -O2 examples/good/good_kthread.c -o /tmp/good_kthread
gcc -Wall -Wextra -Werror -O2 examples/bad/bad_kthread.c -o /tmp/bad_kthread
python tools/lint/skill_lint.py skills/kernel/kthread-create-and-teardown/SKILL.md
python tools/tokens/token_measure.py --check 2000 skills/kernel/kthread-create-and-teardown
```

Target (kernel) — documented only, NOT run here:

```
# module load/unload loop with the kthread, lockdep + KASAN enabled
make defconfig && make -j$(nproc) && make modules

# kthread_stop() from module_exit; verify the thread's exit state
# via /proc/PID/task/TID/stat after each unload (no zombie/ghost threads)
lsmod && rmmod my_driver && grep State /proc/PID/task/TID/stat
```

## Scoring

- precision: every flagged pattern maps to a real kthread contract rule
  (stop-while-alive, poll-the-flag, stop-before-free).
- recall: each bad snippet is detected.
- FP-rate: good snippets produce zero flags.
