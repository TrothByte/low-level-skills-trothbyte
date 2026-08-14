# Vectorization Reasoning — Foundational Loop Analysis

Source-backed rules for `vectorization-reasoning`. Each rule follows
RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE →
VERIFICATION → SOURCE. Facts marked **OBSERVED** were verified on GCC 16.1.0
(MSYS2 MinGW, x86-64, generic target) with `examples/good/loops.c` and
`examples/bad/loops.c`.

## 1. What each `-fopt-info` report means

- **RULE**: GCC `-fopt-info-vec` lists only loops the vectorizer SUCCEEDED
  on. Blockers appear only under `-fopt-info-missed-vec`. The two flags cannot
  be combined in one command (GCC warns "ignoring possibly conflicting
  option"). `-fopt-info-vec` dumps append to an existing file instead of
  truncating it.
- **WHY AI GETS IT WRONG**: runs only `-fopt-info-vec`, gets an EMPTY file,
  and concludes "the loop cannot be vectorized, no reason given" or "the flag
  is broken". Actually `-fopt-info-vec` reports only success; the reason for
  failure lives in the missed dump.
- **CORRECT REASONING**: success evidence = `-fopt-info-vec`; blocker
  evidence = `-fopt-info-missed-vec`. Run them as two separate compiles, and
  delete the dump file (or vary its name) before each compile because GCC
  appends.
- **EXAMPLE** (bad): `gcc -O2 -fopt-info-vec=out.txt -S blocked.c` produces an
  empty `out.txt`; agent reports "not vectorized, no reason given".
- **COUNTEREXAMPLE** (good): `gcc -O2 -fopt-info-missed-vec=out.txt -S
  blocked.c` names the blocker, e.g. `not vectorized, possible dependence
  between data-refs *_5 and *_2`.
- **VERIFICATION**: OBSERVED — `-O2` on `examples/bad/loops.c` gives an empty
  vec dump (0 bytes) while the missed dump names each blocker; `-O2` on
  `examples/good/loops.c` gives `optimized: loop vectorized using 16 byte
  vectors and unroll factor 4` for the loops that succeed. Reusing one dump
  filename across two compiles doubles its line count.
- **SOURCE**: `gcc-manual` (Options for Debugging Your Program or GCC,
  `-fopt-info`); `clang-docs` (UsersManual Diagnostic Options, `-Rpass`).

## 2. Loop-carried dependencies

- **RULE**: a dependence is loop-carried when a value computed in iteration
  `i` is read in iteration `i+k`. A distance-1 serial chain such as
  `a[i] += a[i-1]` is not vectorizable as written at any `-O` level, on any
  ISA, because vectorization reorders independent iterations and the ordering
  here is semantically required.
- **WHY AI GETS IT WRONG**: "`sum += a[i]` is loop-carried too, so a prefix
  sum must vectorize" — the agent conflates a recognized reduction pattern
  with a general recurrence. They are different things (see rule 5).
- **CORRECT REASONING**: ask whether iteration `i` reads what iteration `i-1`
  wrote. If yes, the loop is a scan/recurrence: no legal vectorization as
  written; a parallel scan (Blelloch-style) is an algorithm change, not a
  compiler flag. Adding `restrict` does not help — the dependency is within
  one array, not between two pointers.
- **EXAMPLE** (bad): `void bad_prefix(int32_t *a, size_t n) { for (size_t i =
  1; i < n; ++i) a[i] += a[i - 1]; }` — OBSERVED at `-O2`: `not vectorized,
  possible dependence between data-refs *_5 and *_2`.
- **COUNTEREXAMPLE** (good): `for (size_t i = 0; i < n; ++i) sum += a[i];`
  — a reduction, OBSERVED vectorized at `-O3` with an xmm accumulator and a
  horizontal `psrldq $8` / `psrldq $4` + `paddd` epilogue.
- **VERIFICATION**: compile `examples/bad/loops.c` `bad_prefix` at `-O2`,
  `-O3`, and `-O2 -mavx2`; the missed dump reports the data dependence at
  every level.
- **SOURCE**: `gcc-manual` (vectorizer dependency analysis); `intel-opt-manual`
  (loop-carried dependencies, vectorization guidance); `agner-fog` (cost of
  serial chains).

## 3. Aliasing, `restrict`, and `__restrict__`

- **RULE**: a loop that writes through one pointer and reads through another
  is not vectorized unless the compiler can prove disjointness. `restrict`
  (C11 §6.7.3.1; GCC/Clang extension `__restrict__` for C++) is that proof.
  `const` on a pointee is not. Violating `restrict` is UB.
- **WHY AI GETS IT WRONG**: "`const int32_t *b` can't alias `int32_t *c`" —
  `const` qualifies the agent's view through `b`, not the underlying object;
  the object may be non-const and reachable through `c`. GCC must assume
  overlap and keep the store before the load.
- **CORRECT REASONING**: `c[i] = b[i] + 1` writes `*c`; without `restrict`
  the write could clobber what `b` points to, so a later `b[i]` load could
  observe the store. `restrict` moves that proof to the caller: both buffers
  must really be disjoint, otherwise results are wrong, not slow. Prefer
  `restrict` over pragmas because it is part of the function contract.
- **EXAMPLE** (bad): `bad_alias` in `examples/bad/loops.c` — OBSERVED at
  `-O2`: `missed: no stmts to vectorize.`; at `-O3` the same loop is rescued
  by runtime alias versioning: `loop versioned for vectorization because of
  possible aliasing`.
- **COUNTEREXAMPLE** (good): `good_restrict` — identical body with `restrict`
  on both parameters — OBSERVED vectorized at `-O2`.
- **VERIFICATION**: compare the two functions in the example files; at `-O3`
  the bad twin reports versioning while the good twin has no versioning line.
  Runtime equality check (`RUN_CORRECTNESS`) prints `OK`.
- **SOURCE**: `iso-c11-n1570` §6.7.3.1 (`restrict`), §6.5p7 (aliasing);
  `gcc-manual` (`-fstrict-aliasing`, `__restrict__`, `-fopt-info` reports).

## 4. Trip count: known vs unknown

- **RULE**: the vectorizer must compute how many vector iterations to run. An
  affine bound (`i < n` with `n` loop-invariant) is sufficient — the count is
  computable before execution. A data-dependent exit condition (`while (i < n
  && a[i] > limit)`) is not, so the loop stays scalar.
- **WHY AI GETS IT WRONG**: "runtime `n` means unknown trip count, so it
  can't vectorize" — a runtime but affine bound is fine and ubiquitous; and
  the reverse mistake, assuming a data-dependent `while` is vectorizable
  because `n` is passed in.
- **CORRECT REASONING**: distinguish the bound's FORM from its VALUE. What
  blocks the vectorizer is an exit that depends on the data being processed,
  not on a runtime bound. When semantics allow, rewrite a data-dependent
  early-exit loop as a full-range affine loop with a per-element guard
  (`if (a[i] > limit) ...`).
- **EXAMPLE** (bad): `bad_unknown_trip` — OBSERVED `-O2`/`-O3`:
  `missed: no stmts to vectorize.` The trip count is unknowable in advance.
- **COUNTEREXAMPLE** (good): `good_affine_guard` — full-range loop with a
  per-element guard — OBSERVED vectorized at `-O2 -mavx2` (`32 byte vectors
  and unroll factor 8`); it stays scalar on plain SSE2 because the
  if-conversion is not profitable under the default cost model.
- **VERIFICATION**: `bad_unknown_trip` never appears in a vec dump; with
  `-mavx2` `good_affine_guard` does. Predict this BEFORE compiling.
- **SOURCE**: `gcc-manual` (vectorizer trip-count analysis); `intel-opt-manual`
  (iteration-count-driven optimization).

## 5. Reductions are a recognized pattern

- **RULE**: `sum += a[i]` with a scalar accumulator and an associative,
  commutative operator is a reduction: the compiler reassociates the chain,
  keeps a vector of partial sums, and reduces them in the epilogue.
- **WHY AI GETS IT WRONG**: "reductions are loop-carried, so they cannot
  vectorize" (the opposite of rule 2's error) — or claiming a distance-1
  prefix sum is "a reduction". A reduction's iterations are independent under
  reassociation; a scan's are not.
- **CORRECT REASONING**: reductions are one of the few patterns the vectorizer
  handles specially. It is not a hint to the programmer to unroll manually;
  the accumulator lives in a vector register and the horizontal reduction is
  generated for you. Float reductions need `-ffast-math` (or `#pragma omp
  simd reduction(+:s)`) because reassociation changes FP rounding.
- **EXAMPLE** (bad): treating `sum += a[i]` as a serial dependency and
  concluding it must be hand-unrolled or rewritten with intrinsics.
- **COUNTEREXAMPLE** (good): `good_reduction` — OBSERVED at `-O3`:
  `loop vectorized using 16 byte vectors and unroll factor 4`; asm shows
  `pxor %xmm0, %xmm0` (zeroed accumulator), `paddd`, then the epilogue
  `movdqa %xmm0,%xmm1; psrldq $8, %xmm1; paddd; movdqa; psrldq $4; paddd`.
- **VERIFICATION**: `grep -E "pxor|psrldq|paddd" loops.s` on the `-O3` build
  of `examples/good/loops.c`; the four-element horizontal sequence proves the
  reassociation.
- **SOURCE**: `gcc-manual` (vectorizer reduction support); `intel-opt-manual`
  (reduction optimization); `agner-fog` (cost of the horizontal epilogue);
  `intel-intrinsics` (the equivalent `_mm_hadd_epi32`/`psrldq` operations).

## 6. Induction variables: affine vs not

- **RULE**: the vectorizer requires each per-iteration memory address to be an
  affine function of the loop counter: `base + i*stride`. Non-affine access
  patterns — `a[i*i]`, `a[table[i]]` (gather), write-side gathers (scatter) —
  cannot be turned into contiguous or strided vector accesses.
- **WHY AI GETS IT WRONG**: "it's just an index, the compiler should handle
  it" — an index that is data-dependent or polynomial per iteration is a
  completely different access shape; most vectorizers reject it (AVX-512
  gather/scatter are the exceptions, and even they are slow).
- **CORRECT REASONING**: compute the access function. `i*2`, `i+3`, `(n-i)`
  are affine and fine. `i*i` grows quadratically; `a[i*(i+1)/2]` is triangular
  — no fixed stride. A derived pointer advancing by a constant (`p += 4`) is
  just another induction variable and is fine.
- **EXAMPLE** (bad): `bad_non_affine` — `a[i*i] = b[i] + 1` — OBSERVED at
  `-O2`: `not vectorized: not suitable for scatter store *_6 = _7;`. A
  data-dependent write position is a scatter.
- **COUNTEREXAMPLE** (good): `good_known_bounds` — contiguous `g_c[i] =
  g_a[i] + g_b[i]` with constant trip count — OBSERVED vectorized at `-O2`.
- **VERIFICATION**: the missed dump for `bad_non_affine` names the scatter;
  reading that exact line tells you the address function is non-affine.
- **SOURCE**: `gcc-manual` (vectorizer, scatter/gather handling); `intel-opt-manual`
  (gather/scatter cost); `intel-intrinsics` (gather/scatter intrinsics);
  `agner-fog` (their real throughput).

## 7. Pragmas: `#pragma omp simd`, `#pragma GCC ivdep`, `#pragma clang loop`

- **RULE**: `#pragma omp simd` (with `-fopenmp` or `-fopenmp-simd`) and
  `#pragma GCC ivdep` tell the vectorizer to assume the loop is vectorizable
  and to ignore false dependencies. `#pragma clang loop vectorize(enable)` /
  `vectorize_width(N)` is the Clang-specific analogue. All of them are
  assertions: they can make the loop vectorize, but they do not change the
  semantics of overlapping buffers.
- **WHY AI GETS IT WRONG**: "adding `#pragma omp simd` fixes the aliasing
  blocker safely" — the pragma silences the vectorizer's concern, it does not
  make overlapping buffers legal. Wrong results are the failure mode, and the
  agent also forgets the `-fopenmp-simd` flag so the pragma is silently
  ignored.
- **CORRECT REASONING**: pragmas assert what `restrict` proves. Prefer
  `restrict` where the contract really is disjointness (it is checked at the
  caller site and is part of the type system); use the pragma only when you
  have proven by inspection that the dependence the vectorizer sees is false.
  `#pragma omp simd` is target-independent; `#pragma GCC ivdep` is
  GCC-specific; `#pragma clang loop` is Clang-specific — code carrying the
  wrong one gets it ignored with a warning.
- **EXAMPLE** (bad): `#pragma omp simd` over a loop whose buffers really do
  overlap — vectorized, wrong output, no diagnostic.
- **COUNTEREXAMPLE** (good): `good_ivdep` — aliasing loop + `#pragma GCC
  ivdep` — OBSERVED vectorized at `-O2` (`16 byte vectors and unroll factor
  4`); OBSERVED the same loop with `#pragma omp simd` + `-fopenmp-simd`
  vectorizes too.
- **VERIFICATION**: `gcc -O2 -fopenmp-simd -fopt-info-vec=out.txt -S
  pragma.c` on a `#pragma omp simd` loop; without `-fopenmp-simd` GCC prints
  `warning: ignoring '#pragma omp simd'` and nothing appears in the dump.
- **SOURCE**: `gcc-manual` (OpenMP, `-fopenmp-simd`, `#pragma GCC ivdep`);
  `clang-docs` (`#pragma clang loop`); `intel-opt-manual` (vectorization
  directives).

## 8. Alignment assumptions

- **RULE**: on x86-64 (SSE2+), unaligned SIMD loads/stores (`movdqu`,
  `movups`) are baseline instructions; alignment is a code-quality concern
  (cache-line and page boundaries), not a legality blocker. On targets
  without unaligned vector access (pre-SSE2 x86, older NEON) alignment is a
  hard requirement.
- **WHY AI GETS IT WRONG**: "the loop didn't vectorize because the buffer is
  misaligned" on x86-64 — false; the real blocker is usually cost model or
  dependencies, and agents also misdiagnose `movdqu` output as an alignment
  bug.
- **CORRECT REASONING**: check the target ISA before reasoning about
  alignment. `_Alignas`/`aligned_alloc` matter for intrinsics that require
  aligned operands (`_mm_load_si128`), for cache-line behavior, and for
  targets without unaligned access. The vectorizer uses known alignment to
  pick `movdqa`/`vmovdqa` when profitable, but `movdqu` is not a fallback on
  x86-64 — it is the default.
- **EXAMPLE** (bad): claiming `good_known_bounds` (aligned globals) needs
  `movdqa` to be "properly" vectorized.
- **COUNTEREXAMPLE** (good): OBSERVED — `_Alignas(64)` globals still compile
  to `movdqu`/`movups`; the loop is vectorized regardless. Alignment does not
  gate vectorization on this target.
- **VERIFICATION**: `grep -E "movdqa|movdqu|movups" loops.s` on the `-O2`
  build of `examples/good/loops.c` — vectorized loops use `movdqu`/`movups`,
  no `movdqa` in the load/store path.
- **SOURCE**: `intel-sdm` Vol.2 (`movdqu`/`movups` semantics); `intel-opt-manual`
  (alignment and memory optimization); `iso-c11-n1570` §7.22.3.1
  (`aligned_alloc`); `gcc-manual` (`__builtin_assume_aligned`).

## 9. The cost model decides "should", dependencies decide "can"

- **RULE**: `-ftree-vectorize` is enabled at `-O2`, but the default cost
  model at `-O2` is `very-cheap` and at `-O3` it is `dynamic`
  (`-fvect-cost-model=very-cheap|cheap|dynamic`). Many correct loops vectorize
  only at `-O3` — that is a policy decision, not a bug or a missing pass.
- **WHY AI GETS IT WRONG**: "it vectorizes at `-O3` but not `-O2`, so the
  `-O2` compiler is broken / the loop must be rewritten" — the loop is fine;
  the cost model judged vectorization unprofitable at `-O2`.
- **CORRECT REASONING**: a loop that is scalar at `-O2` and vectorized at
  `-O3` is cost-model-gated. Verify by recompiling with `-O2
  -fvect-cost-model=dynamic` (or `cheap`): if it then vectorizes, the blocker
  was never a dependency. The cost model estimates the vector/scalar speedup
  from instruction latencies and throughput (`agner-fog` data feeds this
  reasoning on x86).
- **EXAMPLE** (bad): `good_reduction` — OBSERVED scalar at `-O2`, vectorized
  at `-O3` and at `-O2 -fvect-cost-model=cheap`. The code is correct at both
  levels.
- **COUNTEREXAMPLE** (good): the same loop at `-O3` produces the xmm
  accumulator and horizontal-reduction epilogue; at `-O2 -fvect-cost-model=
  dynamic` it vectorizes too. Forcing `-O3` globally to get one loop
  vectorized is usually the wrong trade; scope the flag or accept the model.
- **VERIFICATION**: run `gcc -O2` and `gcc -O3` `-fopt-info-vec` on
  `examples/good/loops.c`; the reduction appears only in the `-O3` dump
  (OBSERVED), and appears in the `-O2` dump with `-fvect-cost-model=cheap`.
- **SOURCE**: `gcc-manual` (`-fvect-cost-model`, `-ftree-vectorize`);
  `agner-fog` (latency/throughput tables behind the estimate); `intel-opt-manual`
  (when vectorization pays off).

## Quick decision table

| Missed-dump line | Root cause | Fix |
|---|---|---|
| `no stmts to vectorize.` | analysis gave up: aliasing at `-O2` cost model, or pattern not recognized | add `restrict`; check `-O3`; isolate the loop |
| `possible dependence between data-refs *_5 and *_2` | aliasing, or distance-1 recurrence | `restrict` (aliasing) vs scan algorithm (recurrence) |
| `not suitable for scatter store` | non-affine write index | rewrite index to affine; restructure algorithm |
| loop scalar at `-O2`, vectorized at `-O3` | cost model `very-cheap` | `-fvect-cost-model=dynamic` or accept `-O3` |
| loop scalar everywhere | data-dependent exit / recurrence | full-range + guard; scan algorithm |
