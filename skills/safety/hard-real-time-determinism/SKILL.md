---
name: hard-real-time-determinism
description: Use when writing or reviewing hard real-time code — bare-metal or RTOS tasks with deadlines, WCET analysis, or safety-critical control loops. Enforces no dynamic allocation, no recursion, no exceptions, bounded loops, and deterministic scheduling.
---

# Hard Real-Time Determinism — WCET-Bounded Code

## When to use

- Writing or reviewing bare-metal or RTOS tasks whose deadlines are hard:
  a missed deadline is a system failure, not a quality regression.
- Specifying or checking Worst-Case Execution Time (WCET): every code path
  must have a provable upper bound, not a measured average.
- Adding or reviewing a safety-critical control loop (motor, powertrain,
  medical pump, flight control, power converter) that must close the loop
  every period.
- Routing tasks through a scheduler (FreeRTOS, Zephyr, bare-metal round-robin)
  and needing a schedulability argument instead of "it feels fast".
- Choosing between a convenient pattern and a bounded one: fixed pools versus
  `malloc`, iteration versus recursion, blocking with priority inheritance
  versus unbounded polling.

## When not to use

- Soft real-time or best-effort code (UI, logging, telemetry) where an
  occasional late result is acceptable: the full ban list is over-restrictive.
- Desktop and application code on general-purpose OSes with no deadline
  contract.
- Work whose deliverable is average latency under load: this skill demands a
  worst-case bound; if averages are enough, the discipline still helps but the
  verification burden is disproportionate.
- Frozen, already-certified subsystems: introducing these bans is a redesign,
  not a patch.
- Legacy code that cannot change: apply the review checklist but scope
  remediation as an explicit risk decision.

## What the agent often gets wrong

- Claims "it is fast enough in practice": hard real time requires a provable
  bound, not a measured average; the average is exactly what WCET analysis
  rejects.
- Uses `malloc` once at init and argues "it is only once": allocation time is
  unbounded, the heap fragments, and hidden re-entrancy allocates in task
  context (printf paths, RTOS internals).
- Writes a recursive algorithm and justifies it with "small inputs": the bound
  must hold for the worst input the task can ever see, and the stack is a
  finite resource.
- Treats soft and hard deadlines alike and ports soft-RT optimizations (drop
  frames, retry, adaptive sample rate) into a hard-RT loop.
- Trusts `-O2` as "real-time": compiler transforms create data-dependent
  branches and the timing is never measured with a WCET tool.
- Checks deadlines with `gettimeofday` or the wall clock: the clock jumps
  (NTP, timezone, manual set); deadline logic must use a monotonic source.
- Assumes atomic operations are bounded-time everywhere: compare-exchange and
  lock-prefixed ops can spin or contend, and some targets emulate atomics in
  software.
- Fixes recursion by raising the stack size: that removes the symptom, not the
  unboundedness; the recursion must become iteration.
- Ignores the CPU clock: un-pinned frequency scaling and thermal throttling
  make the WCET a moving target.

## How to reason correctly

1. Classify first: is this task hard or soft real-time? For a hard task every
   statement needs a provable worst-case bound; a construct that cannot be
   bounded is banned.
2. Prove a bound for each construct: allocation -> no; recursion -> no;
   exceptions -> no; a loop whose trip count depends on data -> no; a loop
   with a compile-time or provable bound -> yes.
3. Allocate statically: define every buffer, queue and object at system init;
   use fixed-size pools and preallocated queues. ISR work is deferred to a
   queue, never allocated.
4. Do the math: compute utilization and run response-time analysis with
   measured or derived WCETs. The RMS utilization bound
   `U = n(2^(1/n) - 1)` is sufficient but often pessimistic; response-time
   analysis decides.
5. Protect shared resources with priority-inheriting mutexes or the ceiling
   protocol; keep critical sections short; keep ISRs minimal with bounded
   work.
6. Verify with a cycle counter: instrument entry and exit, record the maximum
   observed time, and confirm with a WCET analyzer (aiT, OTAWA, Heptane) when
   one is available. The measured max is the start, never the proof.
7. Pin the CPU frequency and the cache behavior you rely on; use monotonic
   hardware timers for all deadline logic.

## What to verify

- No `malloc`, `calloc`, `realloc`, `free`, `new` or `delete` in any task or
  ISR code path.
- No recursion: every function is reached via a bounded call chain.
- No C++ exceptions (`throw`/`catch`) in hard real-time paths; error handling
  is status returns.
- Every loop has a provable bound: trip counts are compile-time constants,
  named constants, or bounded counters that move monotonically to the bound.
- All timing uses monotonic hardware timers; no `gettimeofday` or wall clock
  in deadline logic.
- A schedulability analysis is documented (utilization and response times for
  every task) and the WCET used is a measured maximum, never an average.
- Shared resources are protected by priority-inheriting protocols; no
  unbounded blocking (priority inversion) survives review.

## How to verify

Run the bundled heuristic scanner on the example sets (host-verifiable here;
recorded output in `evals/README.md`):

```
python examples/tools/rt_banned_patterns.py examples/bad/recursive_task.c
python examples/tools/rt_banned_patterns.py examples/bad/malloc_in_task.c
python examples/tools/rt_banned_patterns.py examples/bad/unbounded_loop.c
python examples/tools/rt_banned_patterns.py examples/good/bounded_task.c
```

Run the schedulability model (host):

```
python examples/tools/wcet_budget.py --scenario schedulable
python examples/tools/wcet_budget.py --scenario overloaded
```

Compile and run the good fixture, recording the measured per-frame time and
the deadline verdict:

```
gcc -Wall -Wextra -Werror -O2 examples/good/bounded_task.c -o bounded_task.exe
./bounded_task.exe
```

Target gate (documented, not run on this host — WCET analyzers are
target-specific): run aiT, OTAWA or Heptane on the binary; trace the
scheduler with an RTOS trace tool; confirm the max measured time on a logic
analyzer against the analysis.

The python scanner is a fast review loop; it is a heuristic and never proof of
hard real-time behavior.

## Where the knowledge comes from

- MISRA C:2012 rule 21.3 / AUTOSAR C++14 — resource and determinism rules
- Certification: DO-178C / ISO 26262 timing requirements
- WCET analysis tools — aiT (AbsInt, https://www.absint.com/), OTAWA (https://www.otawa.fr/), Heptane (https://team.inria.fr/alf/software/heptane/)
- FreeRTOS scheduler docs (https://www.freertos.org/Documentation/02-Kernel/04-API-references/01-Task-creation/00-Task-creation)
- Fixed-priority scheduling — Rate Monotonic Analysis (Liu & Layland 1973)

## Related skills

- `rtos-concurrency-and-isr`
- `misra-c-compliance`
- `embedded-interrupt-and-nested`
- `embedded-linker-script`
- `embedded-volatile-and-memory-ordering`
- `performance-measurement-discipline`
- `c-errno-and-syscall-returns`

## Evaluation

- Synthetic: the scanner must flag every banned construct in `examples/bad/`
  (recursion, malloc/free/calloc/realloc in task context, data-dependent
  loops, `strlen` over input) and approve `examples/good/bounded_task.c`; the
  good fixture must compile with `-Werror`, run, and report every frame inside
  its deadline.
- False-positive: bounded loops that merely look unbounded must NOT be flagged
  — a `for` loop with a named-constant bound, a `while (items != 0)` counting
  loop that decrements, and a blocking `for (;;)` task superloop with a wait
  call must pass the scanner.
- Historical: Therac-25-style timing safety incidents and the Mars Pathfinder
  priority inversion — the agent must be able to explain how unbounded
  blocking and unanalyzed timing turn into failures.
- Adversarial: a file that hides `malloc` behind a helper, recursion behind a
  function pointer, or an unbounded loop behind a `do/while` must be caught; a
  claim of hard real time without WCET measurement must be rejected.
- Host runs with real output: `evals/README.md`.
