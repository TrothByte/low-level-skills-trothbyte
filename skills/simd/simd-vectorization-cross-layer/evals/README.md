# Evaluation — simd-vectorization-cross-layer

Skill: `skills/simd/simd-vectorization-cross-layer`. Stability target: `evaluated`.

## Verified facts (OBSERVED, GCC 16.1.0, MSYS2 MinGW, x86-64, generic target)

All commands below exit 0. Full transcripts and asm are regenerable with the
commands in this file; source files are `examples/good/loop.c` and
`examples/bad/loop.c`.

| Fact | Evidence |
|---|---|
| `-fopt-info-vec` reports only success | good file: `optimized: loop vectorized using 16 byte vectors and unroll factor 4`; bad file: EMPTY dump |
| failures need `-fopt-info-missed-vec` | bad file: `missed: not vectorized, possible dependence between data-refs *_5 and *_2` |
| dump files append, not truncate | two compiles into the same dump → doubled line count |
| `restrict` removes the aliasing blocker | `bad_alias` scalar at `-O2`; `good_alias` (same body + restrict) vectorized |
| loop-carried dependency never vectorizes | `a[i] += a[i-1]` blocked at `-O2`, `-O3`, `-O2 -mavx2` |
| `-O2` uses the `very-cheap` cost model | `c[i]=b[i]*2`, `c[i]=b[i]+x`, pure copy scalar at `-O2`; all vectorized at `-O3` and at `-O2 -mavx2` |
| reduction vectorizes at `-O3` | asm: `pxor` accumulator + `paddd` + `psrldq`/`paddd` horizontal reduction |
| `-O3` adds runtime alias versioning | `good_assume_aligned` (no restrict): `optimized: loop versioned for vectorization because of possible aliasing` |
| unaligned SIMD is free on x86-64 | aligned `_Alignas(64)` globals and `__builtin_assume_aligned` still compile to `movdqu`/`movups`, never `movdqa` |
| misaligned store is not a hard blocker | `c[i+1] = b[i]*2` vectorizes at `-O3` with `movups %xmm0, 4(%rcx,%rax)` |
| vector results equal scalar results | `RUN_CORRECTNESS` builds of both files print `OK`, exit 0 |

## Verification commands and actual output

```
# Canonical pair (task command + the missed dump that explains the bad side):
gcc -O2 -fopt-info-vec=vec.txt -S examples/good/loop.c
gcc -O2 -fopt-info-vec=vec_bad.txt -S examples/bad/loop.c
gcc -O2 -fopt-info-missed-vec=vec_bad_missed.txt -S examples/bad/loop.c
```

`vec.txt` (good, exact content):

```
examples\good\loop.c:22:26: optimized: loop vectorized using 16 byte vectors and unroll factor 4
examples\good\loop.c:39:26: optimized: loop vectorized using 16 byte vectors and unroll factor 4
```

`vec_bad.txt` (bad): empty file, exit 0 — expected: no loop vectorized.

`vec_bad_missed.txt` (bad, exact content):

```
examples\bad\loop.c:23:26: missed: couldn't vectorize loop
examples\bad\loop.c:23:26: missed: no stmts to vectorize.
examples\bad\loop.c:33:26: missed: couldn't vectorize loop
examples\bad\loop.c:34:18: missed: not vectorized, possible dependence between data-refs *_5 and *_2
examples\bad\loop.c:44:18: missed: couldn't vectorize loop
examples\bad\loop.c:44:18: missed: no stmts to vectorize.
examples\bad\loop.c:57:26: missed: couldn't vectorize loop
examples\bad\loop.c:57:26: missed: no stmts to vectorize.
```

Asm check (xmm/ymm instructions in the generated `loop.s`):

```
grep -E "movdqa|movdqu|movups|paddd|pslld|pcmpeqd|psrld" loop.s
```

Good file, exact vector-instruction lines (both loops, 128-bit xmm):

```
pcmpeqd %xmm1, %xmm1
psrld   $31, %xmm1
movdqu  (%rcx,%rax), %xmm0
paddd   %xmm1, %xmm0
movups  %xmm0, (%rdx,%rax)
movdqu  (%rcx,%rax), %xmm0
paddd   (%rdx,%rax), %xmm0
movups  %xmm0, (%r8,%rax)
```

Bad file: zero xmm/ymm vector instructions in `loop_bad.s` (fully scalar).

Level/target matrix (same two sources):

```
gcc -O3 -fopt-info-vec=o3v.txt -S examples/good/loop.c
gcc -O3 -fopt-info-missed-vec=o3bm.txt -S examples/bad/loop.c
gcc -O2 -mavx2 -fopt-info-vec=avx.txt -S examples/good/loop.c
```

Observed `-O3` good: lines 22, 39, 50 (reduction), 75 (`epilogue loop
vectorized using 8 byte vectors and unroll factor 2`), 88 (`loop versioned for
vectorization because of possible aliasing`). Observed `-O3` bad missed: only
the prefix-sum (33/34) and unknown-trip (44) loops remain blocked. Observed
`-O2 -mavx2` good: lines 22, 39, 50, 62 (affine trip), 75 → `32 byte vectors
and unroll factor 8`.

Runtime correctness (results equal the scalar reference):

```
gcc -O2 -DRUN_CORRECTNESS examples/good/loop.c -o good.exe && ./good.exe   # OK
gcc -O2 -DRUN_CORRECTNESS examples/bad/loop.c -o bad.exe && ./bad.exe      # OK
```

## Synthetic evals

- **easy**: `c[i] = b[i] + 1;` without `restrict` — expected answer: possible
  aliasing between `b` and `c` blocks vectorization; fix = add `restrict`.
- **medium**: `a[i] += a[i - 1];` — expected answer: loop-carried dependency
  (distance 1), not fixable with `restrict`; needs a scan algorithm.
- **medium**: identical loop vectorized at `-O3` but not `-O2` — expected
  answer: `-O2` default cost model `very-cheap`; verify with
  `-fvect-cost-model=dynamic` or accept `-O3`.
- **hard**: `while (i < n && a[i] > limit)` — expected answer: data-dependent
  trip count; reformulate as an affine full-range loop with a per-element
  guard and verify the masked/if-converted version.
- **hard**: `_Alignas(64)` globals / `__builtin_assume_aligned` still yield
  `movdqu` — expected answer: unaligned SIMD is baseline on x86-64; alignment
  is not a vectorization requirement there, and `movdqa` absence is not a bug.

## Adversarial evals

- `__builtin_assume_aligned(p, 16)` present, no `restrict`, loop versioned for
  aliasing at `-O3` — agent must not claim assume_aligned proves disjointness.
- stale dump: recompiling into the same `-fopt-info` file appends old lines —
  agent must delete the dump before trusting line numbers.
- `-fopt-info-vec` returns an empty file — agent must switch to
  `-fopt-info-missed-vec` rather than concluding "no vectorization info".
- a serial prefix sum must NOT be "fixed" with `restrict` or declared a
  compiler bug; it is a fundamentally serial pattern.
- global `-mavx2` producing a binary that traps on AVX1-only CPUs — agent must
  prescribe runtime dispatch, not claim automatic fallback.

## False-positive evals (must NOT flag)

- `c[i] = b[i] + 1` with correct `restrict` and disjoint buffers — valid,
  vectorized; must not be flagged.
- runtime trip count with an affine bound (`i < n`) — vectorizable; must not be
  labeled "unknown trip count".
- `movdqu`/`movups` in generated asm — normal on x86-64; must not be flagged as
  an alignment bug.
- an int reduction scalar at `-O2` — cost-model choice, not a defect; do not
  "fix" it by rewriting without measuring.
- `c[i] = b[i] * 2` scalar at `-O2` — again cost model; the code is correct
  C, vectorization is a policy, not a correctness obligation.
- an intentionally early-exit loop (data-dependent `while`) — correct and
  intentionally scalar; must not be flagged as a missed optimization with a
  behavioral change suggested.

## Scoring

- detection: names the correct blocker (aliasing / loop-carried dependence /
  trip count / cost model) from the missed dump.
- reasoning: predicts the `-fopt-info` line and asm shape BEFORE running the
  compiler.
- fix: changes the source (restrict, affine trip count, target attribute)
  rather than blaming the compiler.
- verification: demonstrates the claim with the vec dump AND the xmm/ymm asm
  lines AND a runtime equality check.

## Sources exercised

`gcc-manual`, `clang-docs`, `intel-sdm`, `intel-opt-manual`, `agner-fog`,
`intel-intrinsics`, `arm-sve-acle`, `iso-c11-n1570`, `sysv-amd64-abi` —
registry ids per `registry/sources.yaml`; full reasoning in
`references/vectorization.md`.
