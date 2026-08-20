# Real-Time Determinism — Reference for LLM Agents

Depth for the `hard-real-time-determinism` skill. SKILL.md is the operational
summary; this file is the reasoning layer.

Uncertainty tags used throughout: KNOWN (established, primary source),
INFERRED (strongly implied by sources), UNVERIFIED (needs target-specific
measurement). Hard real time is defined by the requirement, so all "bounded"
claims below are only as good as the WCET evidence behind them.

## 1. Definitions

- HARD real time: a missed deadline is a system failure. The requirement is
  the worst case: every code path must complete within its budget, always.
- SOFT real time: lateness degrades quality but does not fail the system.
  Averages and percentiles are meaningful; the hard-RT ban list is not.
- WCET: the maximum execution time of a task over all inputs, all cache
  states and all system states reachable in the deployment. Not the max of
  the measurements, but the max the analysis must prove. KNOWN.
- The 100%-load boundary matters: if total utilization exceeds the scheduler
  capacity, no implementation trick fixes it — the work must be reduced or
  the periods extended.

## 2. Banned constructs and why

### 2.1 Dynamic allocation (`malloc`/`free`, `new`/`delete`)

- WHY BANNED: allocation time is implementation-dependent and unbounded in
  the worst case (first-fit scans, coalescing, arena locking); the heap
  fragments so later allocations fail at runtime; ISR context often has no
  usable heap at all. KNOWN.
- "Only at init" is a trap: the call may run in a different context than you
  think (printf, RTOS internal message creation), and the argument cannot be
  verified from one call site. KNOWN (MISRA C:2012 rule 21.3 makes library
  calls an explicit review item; AUTOSAR C++14 restricts dynamic memory).
- CORRECT: static buffers, fixed-size pools, preallocated queues created at
  system init, ISR work deferred to a preallocated queue.

### 2.2 Recursion

- WHY BANNED: stack depth and execution time become a function of the input.
  WCET analysis requires a bounded call graph; a self-call has no static
  bound unless the depth is proven bounded — which is exactly the kind of
  proof that is usually missing. KNOWN.
- The agent "fix" to watch: raising the stack size. The stack is a finite
  resource; the fix must remove the recursion or prove a depth bound.
- CORRECT: transform to iteration with a bounded loop, or bound the recursion
  depth by a constant and prove it.

### 2.3 Exceptions (`throw`/`catch` in C++)

- WHY BANNED: unwinding cost is unbounded and toolchain-dependent; the
  worst-case path through a throwing region is not expressible in a WCET
  model without the runtime's exception tables. KNOWN.
- CORRECT: status-return error handling; every function returns an error code
  and every call site checks it.

### 2.4 Unbounded / data-dependent loops

- WHY BANNED: a loop whose trip count is a function of input data has no
  static bound. `while (buf[i] != 0)` over untrusted input is the classic;
  `strlen`/`strchr` over untrusted data is the same hazard in library form.
  KNOWN.
- CORRECT: loop counters bounded by a compile-time constant, a named
  constant, or a counter that provably moves toward a constant bound; scan
  functions take an explicit maximum length.

### 2.5 Unbounded blocking without priority inheritance

- WHY BANNED: a low-priority task holding a lock can block a high-priority
  task indefinitely (priority inversion). The bounded fix is priority
  inheritance or the priority-ceiling protocol, which bound the blocking
  time to the critical section length. KNOWN (Mars Pathfinder: unbounded
  inversion caused repeated resets — see Section 8).
- ISR rule: ISRs must not block. Defer via a preallocated queue; keep the
  ISR body bounded.

### 2.6 Variable CPU frequency (cpufreq, thermal throttling)

- WHY BANNED: execution time becomes a function of the power state, which is
  not under the task's control. WCET requires the frequency to be pinned or
  the analysis must cover every frequency/voltage state. KNOWN.
- CORRECT: pin frequency at init; use a timer for all timing, never a delay
  calibrated against the current clock.

### 2.7 Unpartitioned cache and thrashing

- WHY BANNED: cache miss behavior is data- and state-dependent; WCET analysis
  must assume worst-case misses unless the cache is partitioned or locked.
  Cache locking makes timing reproducible at the cost of capacity. KNOWN
  (analysis-dependent; aiT models caches conservatively).
- CORRECT: cache locking for the time-critical region, or an analysis that
  bounds the misses; avoid data-dependent access patterns in the hot loop.

### 2.8 Atomics and lock-free retry loops

- WHY BANNED: an atomic retry loop (CAS-spin) has no bounded iteration count
  under contention, and lock-prefixed instructions have contended execution
  times that are hard to bound. On some targets atomics are emulated in
  software. KNOWN/INFERRED.
- CORRECT: prefer a priority-inheriting mutex over a spin; if a lock-free
  structure is required, prove the retry bound or accept a fixed max retries
  as a policy decision.

## 3. Positive patterns

- Fixed-priority preemptive scheduling with a schedulability argument
  (Rate Monotonic for period==deadline, Deadline Monotonic for arbitrary
  deadlines, or EDF where the stack permits).
- Static allocation at init: every buffer and object exists before the
  scheduler starts.
- Bounded loops everywhere in task and ISR paths; loop counters are
  constants or provably bounded counters.
- Priority-inheriting mutexes / ceiling protocol for shared resources.
- Monotonic hardware timer for all deadlines and period generation.
- Measurement: cycle-counter instrumentation recording the MAX, not the
  average, plus a WCET analyzer for the proof.
- Interrupt work minimized: ISR sets a flag and posts to a preallocated
  queue; the task does the work.

## 4. Scheduling analysis

### 4.1 RMS utilization bound (Liu & Layland 1973)

For n independent, synchronous, preemptive tasks with period equal to
deadline, RM is schedulable if:

    U = sum(C_i / T_i) <= n * (2^(1/n) - 1)

- n=2: 0.828, n=3: 0.780, n=4: 0.757, asymptotes to ln 2 = 0.693. KNOWN.
- SUFFICIENT, not necessary. Exceeding the bound does not prove
  unschedulability; the response-time test decides.
- Conditions (deadline==period, no blocking, independent tasks) are almost
  never met exactly in real systems; treat the bound as a first filter.

### 4.2 Response-Time Analysis (Audsley et al.)

For a task i, the worst-case response time is the smallest R satisfying:

    R_i = C_i + sum_{j in hp(i)} ceil(R_i / T_j) * C_j

Iterate R from C_i to a fixed point; if R_i <= D_i, task i meets its
deadline. Blocking B_i (e.g. a non-inheriting lock held by a lower-priority
task) is added to the right-hand side; priority inheritance makes B_i bounded
by the longest critical section. KNOWN.

### 4.3 Priority inheritance vs ceiling

- Priority inheritance (Basic/PIP): a mutex holder temporarily inherits the
  priority of the highest-priority waiter. Bounded by the critical section,
  but chained inheritance is possible. KNOWN.
- Priority ceiling (PCP / Immediate): the holder runs at the ceiling priority
  of the resource, preventing deadlock and third-party interference. Simpler
  to analyze, more pessimistic. KNOWN.
- FreeRTOS `xSemaphoreCreateMutex` implements priority inheritance. KNOWN
  (FreeRTOS scheduler documentation).

## 5. Timing sources

- Deadline logic and period generation MUST use a monotonic clock
  (CLOCK_MONOTONIC, or the target's hardware tick timer). The wall clock
  jumps with NTP/timezone/manual set and can move backward. KNOWN.
- `gettimeofday`/wall-clock deadlines are a review failure. UNVERIFIED as a
  rule is not needed: it is KNOWN that POSIX wall clocks are adjustable.
- Timer-driven, not delay-calibrated, period generation: recalibrate against
  the timer, never against the CPU clock.

## 6. Verification

- Cycle counter / DWT cycle register: instrument task entry and exit; record
  the max observed time over many runs with worst-case inputs.
- The measured max is evidence, not proof: analysis must cover the states the
  tests did not reach.
- WCET analyzers: aiT (AbsInt) — static, hardware-timing based; OTAWA — open
  framework (Abstract Interpretation); Heptane — open, static + measurement
  hybrid. KNOWN (vendors' documentation).
- RTOS trace (SystemView, FreeRTOS-aware tracing, Zephyr tracing): confirm
  that measured run-times match the model and that no task overruns.
- Logic analyzer on a GPIO toggle: ground-truth timing independent of the
  target's own clock.
- Schedulability review: for each task, the analysis, the WCET source, and
  the blocking budget must be documented.

## 7. Therac-25-style lessons (timing and safety)

- Therac-25: race conditions and a missing interlock allowed the treatment
  beam to operate in an unsafe configuration; the interface showed a
  "MALFUNCTION" that the operator could dismiss. The lesson for this skill:
  shared-state races in a time-critical loop are safety hazards, and a
  scheduler argument must prove which task can run when. INFERRED (widely
  documented incident; not a scheduling textbook example).
- Mars Pathfinder (1997): a low-priority task's mutex caused repeated resets
  of the spacecraft through unbounded priority inversion; the fix (enable
  priority inheritance) restored the system. This is the canonical real-world
  demonstration of Section 2.5. KNOWN.

## 8. Sources

- Liu & Layland, "Scheduling Algorithms for Multiprogramming in a
  Hard-Real-Time Environment", JACM 1973 — the RMS utilization bound.
- Audsley, Burns, Richardson, Tindell, Wellings — response-time analysis.
- MISRA C:2012 rule 21.3 (standard library use review) / AUTOSAR C++14
  (dynamic memory restrictions).
- FreeRTOS scheduler documentation — task creation, mutexes with priority
  inheritance.
- DO-178C / ISO 26262 — timing requirements in certification.
- AbsInt aiT (https://www.absint.com/), OTAWA (https://www.otawa.fr/),
  Heptane (https://team.inria.fr/alf/software/heptane/).
- JPL/NASA Mars Pathfinder priority inversion post-mortem reports.
