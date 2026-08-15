# OpenMP Parallel Programming — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to `registry/sources.yaml`.

## 1. Data-sharing attributes: shared by default, races are the default bug

- **RULE**: in a `parallel` region, most variables are `shared` by default
  (loop variables of the canonical loop and loop-invariant automatic vars are
  private per spec). A variable written by multiple threads without
  synchronization is a data race (UB per the C/C++ memory model; OpenMP inherits
  it).
- **WHY AI GETS IT WRONG**: assumes "parallel for makes everything private";
  writes `sum += x[i]` into a shared `sum`; treats the result as "approximately
  right".
- **CORRECT REASONING**: default-sharing is `shared`. Accumulators must be
  `reduction`, private scratch must be `private`/`firstprivate`, and shared
  writes need `atomic`/`critical`. A race is UB, not a performance issue.
- **EXAMPLE** (bad):
  ```c
  int sum = 0;
  #pragma omp parallel for
  for (i = 0; i < n; i++) sum += a[i];   // race on shared sum
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  int sum = 0;
  #pragma omp parallel for reduction(+:sum)
  for (i = 0; i < n; i++) sum += a[i];   // per-thread copy, combined at end
  ```
- **VERIFICATION**: compiled and RUN on this machine — the race yields a wrong
  total, the reduction yields the correct one (recorded in `evals/README.md`).
- **SOURCE**: `openmp-spec` §2.21.1.1 (sharing attributes), §5.1 (memory model).

## 2. reduction: private copy + combine, not "shared +="

- **RULE**: `reduction(operator:var)` creates a private copy per thread, each
  initialized to the operator's identity (0 for +, 1 for *, INT_MAX/MIN for
  min/max), and combines at the end in a deterministic order for most
  implementations (order is not guaranteed by the spec for float sums). Inside
  the loop the variable is the thread's private copy.
- **WHY AI GETS IT WRONG**: reads the reduction variable inside the loop expecting
  the global partial value; uses `reduction` on a variable that also needs
  firstprivate initialization; assumes exact float reduction order.
- **CORRECT REASONING**: the reduction variable is private inside the region.
  Floating-point sums are not guaranteed bit-exact across schedules/orders — use
  `reduction` for numerical stability over a race, but don't promise bit-exact
  results. For user types, `#pragma omp declare reduction`.
- **EXAMPLE** (bad): `reduction(+:x)` and inside the loop `if (x > threshold)` —
  `x` is the thread's private copy, not the running global.
- **COUNTEREXAMPLE** (good): accumulate privately, read the combined value after
  the region.
- **VERIFICATION**: compiled and run; correct result recorded.
- **SOURCE**: `openmp-spec` §2.19.5.4 (reduction), §2.21.5.1 (declare reduction).

## 3. Schedules: correctness is order-independent; cost differs

- **RULE**: `static` assigns iterations to threads at start (chunk contiguous,
  cyclic if chunk set); `dynamic`/`guided` assign at runtime from a work queue;
  `guided` shrinks chunk size geometrically; `auto`/`runtime` delegate. The loop
  may execute iterations in ANY order — correctness must not depend on it.
- **WHY AI GETS IT WRONG**: assumes static is "the default and fine"; relies on
  iteration order for correctness (e.g. an in-loop dependency); picks a schedule
  without a load-balance rationale.
- **CORRECT REASONING**: for an order-independent accumulation (reduction), any
  schedule is correct; pick by cost: static for uniform work, dynamic for
  imbalanced, guided for decreasing. An iteration-order dependency (e.g. writing
  `out[i]` then reading `out[i-1]` across threads) is a bug regardless of schedule.
- **EXAMPLE** (bad): a loop that reads the previous iteration's element and relies
  on static ordering.
- **COUNTEREXAMPLE** (good): the loop writes disjoint outputs or uses a reduction;
  schedule chosen for balance.
- **VERIFICATION**: run the same program with `OMP_SCHEDULE=static` and
  `dynamic` — results must be identical for correct code.
- **SOURCE**: `openmp-spec` §2.9.2 (worksharing loop), §5.1 (schedule clauses).

## 4. atomic vs critical vs private: pick by operation shape

- **RULE**: `#pragma omp atomic` is valid for a single update of one lvalue
  (`x op= expr`, `x++`, `++x`, `--x`, `x--`, and C/C++ capture forms); it is
  cheaper than `critical`. `critical` serializes an arbitrary statement sequence.
  A private variable needs no lock at all.
- **WHY AI GETS IT WRONG**: wraps a multi-statement update in `atomic` (illegal or
  wrong); uses `critical` inside a tight loop for a simple increment (correct but
  slow); adds locking to a private variable.
- **CORRECT REASONING**: classify the update: single lvalue RMW → `atomic`;
  several statements or multiple lvalues → `critical`; thread-local scratch →
  `private`/`firstprivate`. Also note `#pragma omp atomic seq_cst` for
  seq-cst semantics when needed.
- **EXAMPLE** (bad):
  ```c
  #pragma omp atomic
  { tmp = x; x = tmp + f(); }        // compound statement — not an atomic op
  ```
- **COUNTEREXAMPLE** (good): `#pragma omp atomic; x += f();` or a `critical`.
- **VERIFICATION**: compiled and run; the bad form fails to compile or is flagged.
- **SOURCE**: `openmp-spec` §2.17.7 (atomic), §2.17.1 (critical).

## 5. private vs firstprivate vs lastprivate

- **RULE**: `private(x)` makes `x` uninitialized in each thread; `firstprivate(x)`
  copies the entering value into each thread; `lastprivate(x)` copies the value
  from the final loop iteration/section back out (worksharing constructs).
- **WHY AI GETS IT WRONG**: reads a `private` var before writing it (UB);
  uses `lastprivate` on a non-worksharing construct; expects `private` to
  preserve the outer value.
- **CORRECT REASONING**: pick by data flow: seed each thread with the outer value
  → `firstprivate`; thread-local scratch only → `private`; propagate the last
  iteration's value out → `lastprivate` (worksharing only).
- **EXAMPLE** (bad): `private(tmp)` then `if (tmp == 0)` — reads uninitialized.
- **COUNTEREXAMPLE** (good): `firstprivate(seed)` then use.
- **VERIFICATION**: compiled; the uninitialized read is flagged by
  `-Wmaybe-uninitialized` (host run).
- **SOURCE**: `openmp-spec` §2.21.1.2 (private/firstprivate/lastprivate).

## 6. Target offload: map what the device dereferences

- **RULE**: in a `target` region, variables used inside are `map`ped by default
  (`tofrom` for scalars/arrays present in both), but a pointer dereferenced on the
  device must be mapped (the pointed-to data) or be a device pointer
  (`use_device_ptr`/`use_device_addr`). Missing mapping gives an invalid device
  access.
- **WHY AI GETS IT WRONG**: passes a host pointer into the region and dereferences
  it on the device; forgets that the array pointed to is not automatically mapped
  when only the pointer variable is.
- **CORRECT REASONING**: trace every dereference: `#pragma omp target map(tofrom:
  arr[0:n])` for the array; scalars via `firstprivate`/`map(to:)`; device
  pointers via `omp target data use_device_ptr`. Without a mapping the device
  sees a dangling host address.
- **EXAMPLE** (bad):
  ```c
  #pragma omp target
  { for (i = 0; i < n; i++) a[i]++; }   // a is a host pointer, unmapped
  ```
- **COUNTEREXAMPLE** (good):
  ```c
  #pragma omp target map(tofrom: a[0:n])
  { for (i = 0; i < n; i++) a[i]++; }
  ```
- **VERIFICATION**: documentary here — no GPU/offload toolchain on this machine
  (`gcc -fopenmp` is host-only).
- **SOURCE**: `openmp-spec` §2.15 (target constructs), §5.6 (map clause).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Sharing | shared by default; races are the default bug |
| Reduction | private copy + combine; identity init; declare reduction for types |
| Schedules | static/dynamic/guided; correctness must be order-independent |
| atomic | single-lvalue RMW only; compound → critical |
| private | uninitialized in region; firstprivate copies in; lastprivate copies out |
| Target | map everything dereferenced on the device; use_device_ptr for device pointers |
