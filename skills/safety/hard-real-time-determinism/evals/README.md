# Evaluation — hard-real-time-determinism

Skill: `skills/safety/hard-real-time-determinism`.
Stability: `researched` — WCET analysis is target-specific and no WCET
analyzer runs on this host, but the bundled python checker, the
schedulability model and the C fixtures were actually executed here on
2026-08-20 and the output was recorded below.
Toolchain: gcc (Rev5, Built by MSYS2 project) 16.1.0, Python 3.11.9, Windows.

## Verified facts (host, recorded 2026-08-20)

All commands were executed from the repository root.

### 1. Heuristic scanner on the bad fixtures (expect findings, exit 1)

```
python skills/safety/hard-real-time-determinism/examples/tools/rt_banned_patterns.py
     skills/safety/hard-real-time-determinism/examples/bad/recursive_task.c
     skills/safety/hard-real-time-determinism/examples/bad/malloc_in_task.c
     skills/safety/hard-real-time-determinism/examples/bad/unbounded_loop.c
```

Actual output (exit code 1):

```
skills/safety/hard-real-time-determinism/examples/bad/recursive_task.c: 1 banned pattern(s) detected:
  skills/safety/hard-real-time-determinism/examples/bad/recursive_task.c:12: [recursion] function `sum_chain` calls itself (unbounded stack and WCET)
skills/safety/hard-real-time-determinism/examples/bad/malloc_in_task.c: 5 banned pattern(s) detected:
  skills/safety/hard-real-time-determinism/examples/bad/malloc_in_task.c:16: [dynamic-alloc] dynamic allocation `malloc` in a real-time code path
  skills/safety/hard-real-time-determinism/examples/bad/malloc_in_task.c:19: [dynamic-alloc] dynamic allocation `free` in a real-time code path
  skills/safety/hard-real-time-determinism/examples/bad/malloc_in_task.c:27: [dynamic-alloc] dynamic allocation `calloc` in a real-time code path
  skills/safety/hard-real-time-determinism/examples/bad/malloc_in_task.c:31: [dynamic-alloc] dynamic allocation `realloc` in a real-time code path
  skills/safety/hard-real-time-determinism/examples/bad/malloc_in_task.c:32: [dynamic-alloc] dynamic allocation `free` in a real-time code path
skills/safety/hard-real-time-determinism/examples/bad/unbounded_loop.c: 3 banned pattern(s) detected:
  skills/safety/hard-real-time-determinism/examples/bad/unbounded_loop.c:11: [unbounded-loop] data-dependent while loop condition without a bound: *stream !=
  skills/safety/hard-real-time-determinism/examples/bad/unbounded_loop.c:21: [unbounded-loop] data-dependent while loop condition without a bound: has_more_input()
  skills/safety/hard-real-time-determinism/examples/bad/unbounded_loop.c:29: [input-scan] `strlen` scans input with no bound

FAIL: 9 banned pattern(s) across 3 file(s)
```

### 2. Heuristic scanner on the good fixture (expect clean, exit 0)

```
python skills/safety/hard-real-time-determinism/examples/tools/rt_banned_patterns.py
     skills/safety/hard-real-time-determinism/examples/good/bounded_task.c
```

Actual output (exit code 0):

```
skills/safety/hard-real-time-determinism/examples/good/bounded_task.c: OK - no banned patterns detected

PASS: no banned patterns detected across 1 file(s)
```

### 3. Schedulability model, schedulable scenario (exit 0)

```
python skills/safety/hard-real-time-determinism/examples/tools/wcet_budget.py --scenario schedulable
```

Actual output (exit code 0):

```
wcet_budget.py - fixed-priority schedulability model
Scenario: schedulable
Note: control loop + telemetry + watchdog; utilization far below the RMS bound, RTA converges

RMS priority order: control > telemetry > watchdog

Task            C(ms)    T(ms)    D(ms)        U      R(ms)   Deadline  Verdict
control           2.0     10.0     10.0    0.200        2.0       10.0       OK
telemetry         1.0     40.0     40.0    0.025        3.0       40.0       OK
watchdog          0.5     50.0     50.0    0.010        3.5       50.0       OK

Total utilization U = 0.235 (RMS bound for n=3: U_rms = 0.780)
Verdict: SCHEDULABLE (U <= U_rms and every response time <= deadline)
```

### 4. Schedulability model, overloaded scenario (exit 1)

```
python skills/safety/hard-real-time-determinism/examples/tools/wcet_budget.py --scenario overloaded
```

Actual output (exit code 1):

```
wcet_budget.py - fixed-priority schedulability model
Scenario: overloaded
Note: periods cut without removing work; U > 1.0, response times diverge

RMS priority order: control > telemetry > watchdog

Task            C(ms)    T(ms)    D(ms)        U      R(ms)   Deadline  Verdict
control           9.0     10.0     10.0    0.900        9.0       10.0       OK
telemetry         6.0     30.0     30.0    0.200        n/a       30.0     MISS
watchdog          2.0     40.0     40.0    0.050        n/a       40.0     MISS

Total utilization U = 1.150 (RMS bound for n=3: U_rms = 0.780)
Verdict: OVERLOADED (U > 1.0: the work cannot fit the periods)
```

### 5. gcc on the good fixture — clean with -Werror, clean run

```
gcc -Wall -Wextra -Werror -O2 skills/safety/hard-real-time-determinism/examples/good/bounded_task.c -o bounded_task.exe
./bounded_task.exe
```

Actual output: no diagnostics; the binary prints (exit code 0):

```
frames=100000 max_frame_ns=24100 budget_ns=1000000 deadline_missed=0
```

`max_frame_ns=24100` is the measured per-frame max over 100000 frames, an
order of magnitude inside the 1 ms budget; the measured max is the start of
the WCET argument, not the proof.

### 6. gcc on the bad fixtures — legal C that still runs (exit 0)

All three bad fixtures compile with `gcc -Wall -Wextra -O2` and run with
exit code 0, demonstrating that banned code is not broken code: it is
determinism that cannot be proven.

```
skills/safety/hard-real-time-determinism/examples/bad/recursive_task.c  -> prints "sum=36 (recursive)", exit 0
skills/safety/hard-real-time-determinism/examples/bad/malloc_in_task.c  -> prints "ran", exit 0
skills/safety/hard-real-time-determinism/examples/bad/unbounded_loop.c  -> prints "consumed=5 len=5", exit 0
```

## Synthetic evals

- easy/negative: `bad/recursive_task.c` — the self-call of `sum_chain` must
  be flagged as recursion (measured: 1 finding).
- easy/negative: `bad/malloc_in_task.c` — `malloc`, `free` x2, `calloc`,
  `realloc` in task paths must be flagged (measured: 5 findings).
- medium/negative: `bad/unbounded_loop.c` — a data-dependent `while`, a
  function-call `while` condition, and `strlen` over input must be flagged
  (measured: 3 findings).
- easy/positive: `good/bounded_task.c` — static allocation, bounded loops,
  monotonic deadline checks and a priority-inheriting mutex model; the
  scanner must approve it and gcc `-Werror` must build it and run clean
  (measured: both pass).
- medium/positive: the agent must explain why `max_frame_ns` from the
  fixture run is evidence, not proof, and why a WCET analyzer plus the
  schedulability model are required for a claim.

## False-positive evals

The good fixture exercises exactly the patterns that must NOT be flagged
(measured clean):

- `for (i = 0; i < (int)(sizeof(scratch) / sizeof(scratch[0])); i++)` — a
  computed bound from `sizeof` must pass.
- `for (iter = 0; iter < FRAMES; iter++)` — a named-constant bound must pass.
- `while (items != 0) { items--; ... }` — a counting loop that provably
  moves to its bound must pass.
- `for (;;) { if (event_wait() == 0) { ... } }` — a blocking RTOS task
  superloop whose body waits must pass.
- `clock_gettime(CLOCK_MONOTONIC, ...)` — monotonic timing must NOT be
  confused with the banned `gettimeofday`.
- `mutex_lock_pi` / `mutex_unlock` — identifier names containing "lock" must
  not trip the allocation or loop rules.

KNOWN heuristic limitations (documented, not bugs): a `while (p != NULL)`
pointer walk is flagged even when it is provably bounded, because the
scanner cannot prove the bound; a busy loop that merely mentions a
wait-like name in an identifier is accepted. Both need human review.

## Historical evals

- Therac-25 (1985-87): shared-state races in the time-critical treatment
  sequence and a dismissible "MALFUNCTION" message led to fatal overdoses.
  The agent must connect this to this skill: a control loop that shares
  state with other tasks requires a scheduler argument proving which task
  runs when, and a deadline argument proving the safety action is always
  taken in time.
- Mars Pathfinder (1997): a low-priority meteorological task holding a
  mutex blocked the high-priority bus-management task; unbounded priority
  inversion reset the spacecraft repeatedly. The fix was enabling priority
  inheritance. The agent must be able to explain why Section 2.5 of
  `references/real-time-determinism.md` (bounded blocking via priority
  inheritance) is the direct lesson.
- Liu & Layland (1973): the RMS utilization bound `U = n(2^(1/n) - 1)` used
  in `wcet_budget.py` — the agent must be able to state when it is
  sufficient and why response-time analysis is the decisive test.

## Adversarial evals

- A file that hides `malloc` behind a helper (`alloc_hidden`) must still be
  caught. Measured on a trial fixture: 4 findings — `malloc` through the
  helper, `free`, `gettimeofday`, and an unbounded loop hidden behind
  `do/while`, exit 1.
- A C++ trial fixture with `new int[16]`, `delete[]` and `throw` must be
  caught. Measured: 3 findings (new, delete, throw), exit 1.
- Indirect recursion through a function pointer or mutual recursion is NOT
  detected by the heuristic scanner — the agent must admit this and require
  a call-graph/WCET analyzer for the recursion proof.
- A claim "I measured it and it always meets the deadline, so it is hard
  real time" must be rejected: the measured max is evidence; a provable
  WCET and a schedulability analysis are required, and CPU-frequency pinning
  and cache behavior must be part of the argument.

## Verification commands (target — documented, NOT run here)

WCET analyzers are target-specific and are not installed on this host. These
are the documented target gates:

```
# Static WCET analysis (choose the analyzer available for the target)
ait -cpu arm7 ./task_binary            # AbsInt aiT
otawa -p wcet ./task_binary            # OTAWA framework
heptane -s ./task.properties           # Heptane

# RTOS trace: confirm measured task run-times match the model
# e.g. SEGGER SystemView (FreeRTOS), Zephyr tracing

# Logic analyzer on a GPIO toggle around the task body:
#   ground-truth per-period execution time, independent of the CPU clock
```

Expect: every task's analyzed WCET <= the WCET used in `wcet_budget.py`;
traced run-times never exceed the analyzed WCET; the superloop waits instead
of busy-spinning; ISR toggles stay inside their bounded windows.

## Scoring

- Checker precision on the bundled fixtures: 9 true positives on the 3 bad
  fixtures, 0 false positives on the good fixture (measured).
- Schedulability model: 2/2 scenarios give the correct verdict
  (SCHEDULABLE at U=0.235, OVERLOADED at U=1.150) with RTA matching the
  exact fixed-point values by hand (measured).
- Host gate: good fixture compiles with `gcc -Wall -Wextra -Werror -O2`,
  runs 100000 frames and reports no deadline miss (measured).
- Heuristic coverage: dynamic allocation (C and C++ forms), direct
  recursion, exceptions, wall-clock timing, input scans, and unbounded loop
  shapes; indirect recursion and real WCET are out of scope for the scanner
  and must be gated by a call-graph/WCET analyzer.
- Stability is `researched`, not `evaluated`: no WCET analyzer ran on this
  host, and the timing claims in the reference are target-dependent.
