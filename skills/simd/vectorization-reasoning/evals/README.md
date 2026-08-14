# Evaluation — vectorization-reasoning

Skill: `skills/simd/vectorization-reasoning`. Stability target: `evaluated`.

## Verified facts (OBSERVED, GCC 16.1.0, MSYS2 MinGW, x86-64, generic target)

All commands below exit 0. Sources: `examples/good/loops.c` and
`examples/bad/loops.c`. Delete dump files before each compile (GCC appends).

| Fact | Evidence |
|---|---|
| `-fopt-info-vec` reports only success | good file lists 3 loops at `-O2`; bad file dump is 0 bytes |
| blockers need `-fopt-info-missed-vec` | bad file: `not vectorized, possible dependence between data-refs *_5 and *_2` |
| `restrict` removes the aliasing blocker | `bad_alias` scalar at `-O2`; `good_restrict` (same body + restrict) vectorized |
| loop-carried dependency never vectorizes | `a[i] += a[i-1]` blocked at `-O2`, `-O3`, and `-O2 -mavx2` |
| unknown trip count blocks vectorization | data-dependent `while` gives `no stmts to vectorize.` at every level |
| non-affine index is a scatter | `a[i*i] = b[i] + 1` gives `not suitable for scatter store *_6 = _7;` |
| reductions vectorize at `-O3`, not `-O2` | reduction asm: `pxor` accumulator + `paddd` + `psrldq $8`/`psrldq $4` horizontal reduction |
| `-O2` uses the `very-cheap` cost model | reduction and guarded loop scalar at `-O2`; both vectorized at `-O2 -fvect-cost-model=cheap` and at `-mavx2` |
| `-O3` adds runtime alias versioning | `bad_alias` at `-O3`: `loop versioned for vectorization because of possible aliasing` |
| unaligned SIMD is baseline on x86-64 | vectorized loops use `movdqu`/`movups`; no `movdqa` in load/store path even for `_Alignas(64)` globals |
| pragmas assert vectorizability | `#pragma GCC ivdep` and `#pragma omp simd` (+`-fopenmp-simd`) vectorize the no-restrict aliasing loop at `-O2` |
| vector results equal scalar results | `RUN_CORRECTNESS` build prints `OK`, exit 0 |

## Verification commands and actual output

```
gcc -O2 -fopt-info-vec=vec_good.txt -S examples/good/loops.c
gcc -O2 -fopt-info-vec=vec_bad.txt -S examples/bad/loops.c
gcc -O2 -fopt-info-missed-vec=vec_bad_missed.txt -S examples/bad/loops.c
gcc -O3 -fopt-info-vec=vec_good_o3.txt -S examples/good/loops.c
gcc -O2 -mavx2 -fopt-info-vec=vec_good_avx.txt -S examples/good/loops.c
```

`vec_good.txt` (`-O2`, exact content):

```
examples\good\loops.c:19:26: optimized: loop vectorized using 16 byte vectors and unroll factor 4
examples\good\loops.c:59:26: optimized: loop vectorized using 16 byte vectors and unroll factor 4
examples\good\loops.c:72:5: optimized: loop vectorized using 16 byte vectors and unroll factor 4
```

(lines 19, 59, 72 = `good_restrict`, `good_known_bounds`, `good_ivdep`. The
reduction at line 44 and the guarded loop at line 30 are cost-model-scalar.)

`vec_good_o3.txt` (`-O3`, exact content):

```
examples\good\loops.c:19:26: optimized: loop vectorized using 16 byte vectors and unroll factor 4
examples\good\loops.c:44:26: optimized: loop vectorized using 16 byte vectors and unroll factor 4
examples\good\loops.c:59:26: optimized: loop vectorized using 16 byte vectors and unroll factor 4
examples\good\loops.c:72:5: optimized: loop vectorized using 16 byte vectors and unroll factor 4
```

`vec_good_avx.txt` (`-O2 -mavx2`, exact content — all five, 32-byte):

```
examples\good\loops.c:19:26: optimized: loop vectorized using 32 byte vectors and unroll factor 8
examples\good\loops.c:30:26: optimized: loop vectorized using 32 byte vectors and unroll factor 8
examples\good\loops.c:44:26: optimized: loop vectorized using 32 byte vectors and unroll factor 8
examples\good\loops.c:59:26: optimized: loop vectorized using 32 byte vectors and unroll factor 8
examples\good\loops.c:72:5: optimized: loop vectorized using 32 byte vectors and unroll factor 8
```

`vec_bad.txt` (`-O2`): empty file, exit 0 — expected, no loop vectorized.

`vec_bad_missed.txt` (`-O2`, exact content):

```
examples\bad\loops.c:20:26: missed: couldn't vectorize loop
examples\bad\loops.c:20:26: missed: no stmts to vectorize.
examples\bad\loops.c:31:26: missed: couldn't vectorize loop
examples\bad\loops.c:32:18: missed: not vectorized, possible dependence between data-refs *_5 and *_2
examples\bad\loops.c:41:18: missed: couldn't vectorize loop
examples\bad\loops.c:41:18: missed: no stmts to vectorize.
examples\bad\loops.c:52:26: missed: couldn't vectorize loop
examples\bad\loops.c:53:18: missed: not vectorized: not suitable for scatter store *_6 = _7;
```

`-O3` on the bad file resolves only the aliasing case via versioning:

```
examples\bad\loops.c:20:26: optimized: loop vectorized using 16 byte vectors and unroll factor 4
examples\bad\loops.c:20:26: optimized:  loop versioned for vectorization because of possible aliasing
```

Asm check:

```
grep -E "movdqa|movdqu|movups|paddd|pcmpeqd|psrld|pxor|psrldq" loops.s
```

Good file `-O2`, exact vector-instruction lines:

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

Bad file `-O2`: zero xmm/ymm vector instructions (fully scalar).

Reduction at `-O3`, horizontal-reduction epilogue (exact lines):

```
pxor    %xmm0, %xmm0
paddd   %xmm2, %xmm0
movdqa  %xmm0, %xmm1
psrldq  $8, %xmm1
paddd   %xmm1, %xmm0
movdqa  %xmm0, %xmm1
psrldq  $4, %xmm1
paddd   %xmm1, %xmm0
```

Runtime correctness:

```
gcc -O2 -DRUN_CORRECTNESS examples/good/loops.c -o good.exe && ./good.exe   # OK
```

Pragma behavior (temp file with a no-restrict aliasing loop):

```
gcc -O2 -fopenmp-simd -fopt-info-vec=vec_omp.txt -S pragma.c   # #pragma omp simd loop vectorizes
gcc -O2 -fopt-info-vec=vec_ivdep.txt -S pragma.c               # #pragma GCC ivdep loop vectorizes
```

## Synthetic evals

- **easy**: `c[i] = b[i] + 1;` without `restrict` — expected: possible
  aliasing blocks vectorization; fix = add `restrict` on both pointers.
- **medium**: `a[i] += a[i - 1];` — expected: loop-carried dependency
  (distance 1), NOT fixable with `restrict`; needs a scan algorithm.
- **medium**: `sum += a[i];` scalar at `-O2`, vectorized at `-O3` — expected:
  `-O2` default cost model `very-cheap`; verify with
  `-fvect-cost-model=dynamic`, not a code rewrite.
- **hard**: `while (i < n && a[i] > limit)` — expected: data-dependent trip
  count; reformulate as an affine full-range loop with a per-element guard
  and verify the guarded version.
- **hard**: `a[i*i] = b[i] + 1;` — expected: non-affine induction, rejected
  as a scatter store; rewrite the index or the algorithm.

## Adversarial evals

- `#pragma omp simd` presented as a safe fix for genuinely overlapping
  buffers — agent must state it is an assertion that produces wrong results,
  and that `restrict` is the contract-level fix.
- empty `-fopt-info-vec` dump — agent must switch to
  `-fopt-info-missed-vec` instead of concluding "nothing vectorizes".
- stale dump: recompiling into the same `-fopt-info` file appends old lines —
  agent must delete the dump before trusting line numbers.
- `-O2` scalar vs `-O3` vectorized presented as a compiler bug — agent must
  name the cost model and verify with the override flag.
- `const` on a pointee presented as a disjointness proof — agent must reject
  it and require `restrict`.

## False-positive evals (must NOT flag)

- `c[i] = b[i] + 1` with correct `restrict` and disjoint buffers — valid,
  vectorized; must not be flagged.
- runtime trip count with an affine bound (`i < n`) — vectorizable; must not
  be labeled "unknown trip count".
- a reduction scalar at `-O2` — cost-model policy, not a defect; do not
  "fix" it with intrinsics without measuring.
- `movdqu`/`movups` in generated asm — normal on x86-64; must not be flagged
  as an alignment bug.
- an intentionally early-exit data-dependent loop — correct and intentionally
  scalar; must not be "fixed" with a behavioral change.
- a serial prefix sum — correct code; must not be declared a compiler bug or
  "fixed" with `restrict`.

## Scoring

- detection: names the correct blocker (aliasing / recurrence / trip count /
  non-affine index / cost model) from the missed dump.
- reasoning: predicts the `-fopt-info` line and the vector/scalar outcome
  BEFORE running the compiler.
- fix: changes the source (restrict, affine guard, cost-model flag) rather
  than blaming the compiler or rewriting with intrinsics.
- verification: demonstrates the claim with the vec dump, the missed dump,
  the xmm/ymm asm lines, and a runtime equality check.

## Sources exercised

`gcc-manual`, `intel-opt-manual`, `agner-fog`, `intel-intrinsics`,
`iso-c11-n1570`, `clang-docs` — registry ids per `registry/sources.yaml`;
full reasoning in `references/vectorization-reasoning.md`.
