---
name: vectorization-reasoning
description: Use when analyzing whether a C loop can vectorize or why it does not — loop-carried dependencies, aliasing and restrict, known vs unknown trip counts, reductions, induction variables, alignment and cost-model assumptions, and interpreting GCC -fopt-info-vec and missed reports before touching intrinsics.
---

# Vectorization Reasoning — Foundational Loop Analysis

## When to use

- Deciding whether a loop is vectorizable by reasoning about its structure:
  dependencies, trip count, address functions, reductions.
- Diagnosing why a hot loop stays scalar: loop-carried dependency, possible
  aliasing, data-dependent exit, non-affine indexing, or the `-O2` cost model.
- Choosing between `restrict`, `#pragma omp simd` / `#pragma GCC ivdep`,
  rewriting the loop shape, and hand intrinsics.
- Reading GCC `-fopt-info-vec` / `-fopt-info-missed-vec` evidence for a
  vectorization claim.

## When not to use

- Correctness-only C with no performance context (use `c-undefined-behavior`).
- Reading the generated xmm/ymm asm or choosing intrinsics — those are the
  deeper target-level concerns of `simd-vectorization-cross-layer`.
- Profiling and measurement before optimizing (`performance-measurement-discipline`).

## What the agent often gets wrong

- "`-fopt-info-vec` gave an empty file, so nothing can vectorize." It reports
  success only; blockers are in `-fopt-info-missed-vec`.
- "`const int32_t *b` proves `b` and `c` don't overlap." Const on the pointee
  proves nothing; `restrict` is the disjointness contract.
- "A prefix sum is a reduction, so it should vectorize." A distance-1 serial
  chain is a recurrence, not the special reduction pattern.
- "Runtime `n` means the trip count is unknown." Affine bounds (`i < n`) are
  fine; data-dependent exits are not.
- "It vectorizes at `-O3` but not `-O2`, so `-O2` is broken." The `-O2`
  default cost model `very-cheap` declines many profitable loops.
- "The buffer is misaligned, so it can't vectorize." On x86-64 unaligned
  SIMD is baseline; alignment is not the blocker.
- "`#pragma omp simd` fixes aliasing safely." It asserts vectorizability; it
  does not make overlapping buffers legal.

## How to reason correctly

1. Extract the loop body and compute its three facts: (a) does iteration `i`
   read what iteration `i-k` wrote (dependencies)? (b) is the trip count
   computable before execution (affine bound)? (c) is every address an affine
   function of `i` (induction)?
2. Classify the dependency: intra-array distance-1 chain → recurrence (not
   vectorizable); cross-pointer write/read → aliasing, fixable with `restrict`
   if the caller guarantees disjoint buffers.
3. Classify the trip count: affine bound → vectorizable; data-dependent exit
   → reformulate as a full-range loop with a per-element guard if semantics
   allow.
4. Check the access pattern: non-affine index (`i*i`, `table[i]`) → scatter/
   gather, rejected; contiguous or fixed-stride → vectorizable.
5. Distinguish "cannot" from "will not": a loop scalar at `-O2` but vectorized
   at `-O3` is a cost-model decision, not a dependency blocker. Verify with
   `-fvect-cost-model=dynamic` before assuming.
6. Only after steps 1-5, and after reading the missed dump, consider pragmas
   or intrinsics — and keep the loop-level fix (restrict, affine bounds) as
   the primary answer.

## What to verify

- The missed-dump line matches the suspected blocker; do not stop at
  "some loop wasn't vectorized".
- `restrict` is placed only where the caller truly passes disjoint buffers.
- The reformulated (guarded) loop produces the same results as the original.
- Vectorized output equals a scalar reference at runtime.

## How to verify

```
gcc -O2 -fopt-info-vec=vec_good.txt -S examples/good/loops.c
gcc -O2 -fopt-info-vec=vec_bad.txt -S examples/bad/loops.c
gcc -O2 -fopt-info-missed-vec=vec_bad_missed.txt -S examples/bad/loops.c
gcc -O3 -fopt-info-vec=vec_good_o3.txt -S examples/good/loops.c
gcc -O2 -mavx2 -fopt-info-vec=vec_good_avx.txt -S examples/good/loops.c
gcc -O2 -DRUN_CORRECTNESS examples/good/loops.c -o good.exe && ./good.exe
```

Delete each dump file before recompiling (GCC appends to existing dumps).
Observed on GCC 16.1 (x86-64, generic target): the good file reports
`optimized: loop vectorized using 16 byte vectors and unroll factor 4` for
`good_restrict`, `good_known_bounds`, and `good_ivdep` at `-O2`, adding the
reduction at `-O3`; with `-mavx2` all five loops vectorize with `32 byte
vectors and unroll factor 8`. The bad file's vec dump is empty; its missed
dump reports `no stmts to vectorize.` (aliasing at `-O2`, unknown trip),
`possible dependence between data-refs *_5 and *_2` (prefix sum), and `not
suitable for scatter store *_6 = _7;` (non-affine index). The `-O2` bad asm
contains zero xmm/ymm instructions; the reduction asm shows `pxor` + `paddd`
+ the `psrldq $8`/`psrldq $4` horizontal epilogue. The correctness build
prints `OK`.

## Where the knowledge comes from

- `gcc-manual` — `-fopt-info`, `-ftree-vectorize`, `-fvect-cost-model`,
  `-fopenmp-simd`, `#pragma GCC ivdep`, `__restrict__`
- `intel-opt-manual` — vectorization guidance, loop-carried dependencies,
  reduction and memory optimization
- `agner-fog` — instruction cost tables behind the cost model
- `intel-intrinsics` — the gather/scatter and horizontal-reduction intrinsics
  that mirror what the vectorizer generates
- `iso-c11-n1570` — §6.7.3.1 (`restrict` contract), §6.5p7 (aliasing)
- Empirical facts marked OBSERVED were verified on GCC 16.1 (MSYS2 MinGW,
  x86-64, generic target); see `references/vectorization-reasoning.md` and
  `evals/README.md`

## Related skills

- `simd-vectorization-cross-layer` — deeper: reading xmm/ymm asm, runtime
  dispatch, intrinsics, per-ISA alignment (see also)
- `asm-x86-64-registers-and-addressing` — prerequisite register/addressing
  knowledge (require of)
- `performance-measurement-discipline` — measure before optimizing (recommend)
- `cache-and-numa-optimization` — memory-bound performance (recommend)
- `c-undefined-behavior` — aliasing/UB rules underpinning `restrict` (recommend)

## Evaluation

Synthetic: aliasing blocker (easy), loop-carried dependency vs reduction
(medium), unknown trip count and cost-model gating (medium), non-affine
induction / scatter (hard). Adversarial: `#pragma omp simd` as a false cure
for real overlap, empty `-fopt-info-vec` dump, stale appended dumps, treating
`-O2`/`-O3` difference as a bug. False-positive guards: correct `restrict`
loops, affine trip counts, `-O2`-scalar cost-model loops, and intentionally
early-exit loops must not be flagged. Verification commands and the exact
observed `-fopt-info-vec` output are in `evals/README.md`.
