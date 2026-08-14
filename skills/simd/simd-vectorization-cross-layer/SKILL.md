---
name: simd-vectorization-cross-layer
description: Use when reasoning about why a C loop did or did not vectorize, reading GCC `-fopt-info-vec`/`-fopt-info-missed-vec` or Clang `-Rpass=loop-vectorize` output, diagnosing aliasing, loop-carried dependency, alignment, or trip-count blockers, choosing between auto-vectorization, `restrict`, runtime dispatch, and intrinsics, and inspecting xmm/ymm vector asm.
---

# SIMD & Auto-Vectorization Cross-Layer Reasoning

## When to use

- Reading compiler vectorization reports (`-fopt-info-vec`, `-fopt-info-missed-vec`,
  Clang `-Rpass=loop-vectorize`) to explain why a loop did or did not vectorize.
- Diagnosing a hot loop that stays scalar: aliasing, loop-carried dependency,
  unknown trip count, or the `-O2` cost model.
- Deciding between `restrict`, aligned allocation, `-O3`, runtime dispatch
  (`__builtin_cpu_supports`), and hand intrinsics.
- Reviewing SIMD code and inspecting the generated xmm/ymm asm for correctness
  of the claim "this loop is vectorized".

## When not to use

- Correctness-only C questions with no performance context (use
  `c-undefined-behavior` / `safe-low-level-from-scratch`).
- Deep NEON/SVE kernel design (intrinsics deep-dives live in the planned
  `simd-intrinsics-x86` / `simd-neon-sve` skills; `arm-sve-acle` is the source).
- End-to-end application profiling (use `performance-measurement-discipline`).
- Hand-writing SIMD before checking whether auto-vectorization already handles
  the loop — this skill's default is "measure and read the dump first".

## What the agent often gets wrong

- "`-fopt-info-vec` gave an empty file, so the flag is broken / nothing is
  vectorizable." It reports success only; blockers appear under
  `-fopt-info-missed-vec`.
- "`const int32_t *b` means `b` cannot alias `c`." Const on the pointee proves
  nothing about overlap; `restrict` is the disjointness contract.
- "`sum += a[i]` vectorizes, so a prefix sum must too." Reductions are a special
  recognized pattern; a distance-1 serial chain is not reorderable.
- "`-ftree-vectorize` is on at `-O2`, so `-O2` and `-O3` vectorize the same."
  The pass runs, but the default `-O2` cost model (`very-cheap`) rejects many
  profitable loops; `-O3` uses `dynamic`.
- "Vectorization requires 16-byte alignment." False on SSE2+ x86-64: `movdqu`/
  `movups` are baseline and GCC emits them even for `_Alignas(64)` data.
- "Compiling with `-mavx2` falls back automatically on older CPUs." It does
  not; the binary traps with Illegal instruction.
- Reusing the same `-fopt-info` dump filename: GCC appends, stale lines keep
  misleading line numbers.

## How to reason correctly

1. Isolate the loop, compile with `-fopt-info-vec` AND
   `-fopt-info-missed-vec`, and read the blocker message from the missed dump.
2. Classify the blocker:
   - possible data dependence / aliasing → add `restrict` (verify disjoint buffers).
   - loop-carried dependency (`a[i]` reads `a[i-1]`) → not vectorizable as
     written; needs a parallel scan algorithm, not a keyword.
   - data-dependent exit / unknown trip count → reformulate as an affine
     full-range loop with a per-element guard, if semantics allow.
   - loop silently scalar at `-O2` but vectorized at `-O3` → cost model;
     consider `-fvect-cost-model=dynamic` instead of assuming a blocker.
3. Check the target ISA before reasoning about alignment: x86-64 handles
   unaligned SIMD natively; pre-SSE2 and older NEON do not.
4. Reach for intrinsics only for what the vectorizer refuses (cross-lane
   shuffles, gather/scatter, saturating/masked ops), and keep a scalar path.
5. For width-sensitive code, dispatch by CPU capability rather than baking
   `-mavx2` into a baseline binary.

## What to verify

- The missed-dump message matches the suspected blocker (not just "some loop
  wasn't vectorized").
- `restrict` is placed only where the caller guarantees disjoint objects;
  violating it is UB.
- The asm shows vector load → vector op → vector store with the loop counter
  stepping by the vector width, plus a scalar tail.
- Vectorized output equals the scalar reference at runtime — vectorization must
  never change results.

## How to verify

```
gcc -O2 -fopt-info-vec=vec.txt -S examples/good/loop.c
gcc -O2 -fopt-info-vec=vec_bad.txt -S examples/bad/loop.c
gcc -O2 -fopt-info-missed-vec=vec_bad_missed.txt -S examples/bad/loop.c
grep -E "movdqa|movdqu|movups|paddd|pslld" loop.s
gcc -O2 -DRUN_CORRECTNESS examples/good/loop.c -o good.exe && ./good.exe
```

Delete each dump file before recompiling (GCC appends to existing dumps).
Observed on GCC 16.1 (x86-64): good file prints `optimized: loop vectorized
using 16 byte vectors and unroll factor 4` (lines 22, 39); bad file prints an
empty vec dump and `missed: not vectorized, possible dependence between
data-refs *_5 and *_2` etc. in the missed dump; good asm contains
`movdqu`/`paddd`/`movups`, bad asm contains none; both runtime self-checks
print `OK`.

## Where the knowledge comes from

- `gcc-manual` — Optimize Options: `-fopt-info`, `-ftree-vectorize`,
  `-fvect-cost-model`, `__builtin_cpu_supports`, `__builtin_assume_aligned`
- `clang-docs` — `-Rpass=loop-vectorize` / `-Rpass-missed`
- `intel-sdm`, `intel-opt-manual` — SSE/AVX instructions, vectorization guidance
- `agner-fog` — instruction tables and cost reasoning
- `intel-intrinsics`, `arm-sve-acle` — intrinsic semantics, width-agnostic SVE
- `iso-c11-n1570` — §6.7.3.1 (`restrict` contract), §7.22.3.1 (`aligned_alloc`)
- Empirical facts marked OBSERVED were verified on GCC 16.1 (MinGW, x86-64);
  see `references/vectorization.md` and `evals/README.md`

## Related skills

- `asm-x86-64-registers-and-addressing` — prerequisite register/addressing
  knowledge for reading the asm (require of)
- `vectorization-reasoning` — foundational loop-analysis companion (recommend)
- `performance-measurement-discipline` — measure before optimizing (recommend)
- `cache-and-numa-optimization` — the other half of memory-bound performance
- `compiler-ub-assumptions` — aliasing/UB reasoning that underpins `restrict`

## Evaluation

Synthetic: aliasing blocker (easy), loop-carried dependency and `-O2`/`-O3`
cost model (medium), data-dependent trip count and unaligned-access reality on
x86-64 (hard). Adversarial: `__builtin_assume_aligned` vs `restrict`, stale
dump appends, empty vec dump, AVX2 fallback trap. False-positive guards:
correct `restrict` loops, affine trip counts, `movdqu` output, cost-model-scalar
loops, and intentional early-exit loops must not be flagged. Verification
commands and the exact observed `-fopt-info-vec` output are in
`evals/README.md`.
