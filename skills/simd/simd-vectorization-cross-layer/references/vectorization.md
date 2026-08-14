# Auto-Vectorization & SIMD Cross-Layer Knowledge

Source-backed knowledge for `simd-vectorization-cross-layer`. Each rule follows
RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE →
VERIFICATION → SOURCE. Facts marked **OBSERVED** were verified on GCC 16.1
(MSYS2 MinGW, x86-64, generic target) with `examples/good/loop.c` and
`examples/bad/loop.c`.

## 1. Reading vectorization reports: what each flag reports

- **RULE**: GCC `-fopt-info-vec` reports only loops the vectorizer SUCCEEDED
  on. Failures appear only with `-fopt-info-missed-vec`. Clang equivalents:
  `-Rpass=loop-vectorize` (success) and `-Rpass-missed=loop-vectorize`
  (failure). The `-fopt-info` dump selector is `-fopt-info-<kind>-<group>` with
  kind ∈ {optimized, missed, note, all} and group ∈ {vec, loop, slp, ...}.
- **WHY AI GETS IT WRONG**: runs `gcc -O2 -fopt-info-vec=out.txt -S blocker.c`,
  gets an EMPTY file, and concludes "nothing is vectorizable" or "the flag is
  broken", instead of reading the missed dump that names the blocker.
- **CORRECT REASONING**: `-fopt-info-vec` = optimized messages only. The bad
  example produces an empty vec dump by design (no loop vectorized) — the
  reason text lives in the missed dump.
- **EXAMPLE** (bad): `gcc -O2 -fopt-info-vec=out.txt -S loop.c` on a blocked
  loop → empty `out.txt`, agent reports "not vectorized, no reason given".
- **COUNTEREXAMPLE** (good): `gcc -O2 -fopt-info-missed-vec=out.txt -S loop.c`
  → `loop.c:34:18: missed: not vectorized, possible dependence between
  data-refs *_5 and *_2`.
- **VERIFICATION**: OBSERVED — `-fopt-info-vec=vec.txt` on
  `examples/good/loop.c` gives `optimized: loop vectorized using 16 byte
  vectors and unroll factor 4`; on `examples/bad/loop.c` it gives an empty
  file, while `-fopt-info-missed-vec` lists the reasons.
- **SOURCE**: `gcc-manual` (Options for Debugging Your Program or GCC,
  `-fopt-info`); `clang-docs` (UsersManual, Diagnostic Options `-Rpass`).

## 2. Dump files append, they do not truncate

- **RULE**: GCC appends to an existing `-fopt-info=<file>` dump instead of
  overwriting it. Re-using the same dump filename across edits accumulates
  stale lines with wrong source line numbers.
- **WHY AI GETS IT WRONG**: treats the dump as a fresh transcript of the last
  compile; stale `optimized` lines make it believe loops vectorize that the
  current source no longer vectorizes.
- **CORRECT REASONING**: delete the dump file (or vary its name) before each
  compile when inspecting a changed source.
- **EXAMPLE** (bad): edit the source, re-run the same `-fopt-info-vec=vec.txt`
  command, and trust line numbers that refer to the previous source.
- **COUNTEREXAMPLE** (good): `Remove-Item vec.txt` (or `rm vec.txt`) first,
  then compile; each line matches the current source.
- **VERIFICATION**: OBSERVED — compiling the same source twice into the same
  dump doubles its line count (2 lines → 4).
- **SOURCE**: `gcc-manual` (`-fopt-info` documentation).

## 3. The aliasing blocker and `restrict`

- **RULE**: a loop that writes through one pointer and reads through another
  is not vectorized unless the compiler can prove the pointers are disjoint.
  `restrict` (C11 §6.7.3.1) provides that proof. `const` on the read pointer
  does NOT.
- **WHY AI GETS IT WRONG**: "`const int32_t *b` means `b` can't alias `c`" —
  the const qualifier applies to the pointer's pointee view, not to the object;
  the object may be non-const and reachable through `c`. GCC must conservatively
  assume overlap.
- **CORRECT REASONING**: `c[i] = b[i] + 1` writes `*c`; without `restrict`, the
  write could modify what `b` points to, so a later `b[i]` load could observe
  the earlier store. The vectorizer then cannot prove independence. With
  `restrict`, the caller promises disjoint buffers, so the load-store pair is
  independent.
- **EXAMPLE** (bad): `void f(const int32_t *b, int32_t *c, size_t n) { for
  (...) c[i] = b[i] + 1; }` — at `-O2` not vectorized (OBSERVED:
  `missed: no stmts to vectorize`).
- **COUNTEREXAMPLE** (good): same loop with `const int32_t *restrict b,
  int32_t *restrict c` — vectorized at `-O2` (OBSERVED: `loop vectorized using
  16 byte vectors and unroll factor 4`).
- **VERIFICATION**: compare `examples/bad/loop.c` `bad_alias` against
  `examples/good/loop.c` `good_alias` (identical bodies, differ only in
  `restrict`). OBSERVED at `-O3`: GCC emits runtime alias versioning instead —
  `optimized: loop versioned for vectorization because of possible aliasing`.
- **SOURCE**: `iso-c11-n1570` §6.7.3.1 (restrict), §6.5p7; `gcc-manual`
  (Optimize Options, `-fstrict-aliasing`, `-ftree-loop-vectorize`).

## 4. Loop-carried dependency vs. reduction

- **RULE**: a loop-carried dependency where every iteration's output feeds the
  next input (distance-1 dependence, e.g. `a[i] += a[i-1]`) is not vectorizable
  as written. Reductions (`sum += a[i]`) ARE vectorizable because the compiler
  reassociates a single associative accumulator.
- **WHY AI GETS IT WRONG**: "`sum += a[i]` is also loop-carried, so prefix sums
  should vectorize too" — the vectorizer recognizes reductions as a special
  pattern (accumulator + associative op); general serial chains are not
  reorderable.
- **CORRECT REASONING**: vectorization reorders independent operations. In
  `a[i] += a[i-1]` the read depends on the previous iteration's write, so
  reordering changes results. A prefix sum requires a parallel scan
  algorithm, not the loop vectorizer.
- **EXAMPLE** (bad): `for (size_t i = 1; i < n; ++i) a[i] += a[i - 1];` —
  never vectorized (OBSERVED at `-O2`, `-O3`, `-O2 -mavx2`:
  `missed: not vectorized, possible dependence between data-refs *_5 and *_2`).
- **COUNTEREXAMPLE** (good): `int32_t sum = 0; for (...) sum += a[i];` —
  vectorized at `-O3` (OBSERVED: accumulator in `%xmm0`, horizontal
  `psrldq $8` / `psrldq $4` + `paddd` in the epilogue).
- **VERIFICATION**: `gcc -O3 -fopt-info-vec -S` on `examples/good/loop.c`
  `good_reduction`; asm shows `pxor` (zeroed accumulator), `paddd`, and the
  `movdqa`/`psrldq` horizontal reduction sequence.
- **SOURCE**: `gcc-manual` (vectorizer reduction support); `intel-opt-manual`
  (reduction/loop optimization); `agner-fog` (instruction costs for the
  horizontal epilogue).

## 5. Unknown trip count (data-dependent exit)

- **RULE**: a loop whose trip count depends on runtime data — not only loop
  invariants — is not vectorized, because the vectorizer must prove the
  iteration count (or mask every iteration).
- **WHY AI GETS IT WRONG**: "runtime `n` is fine because GCC versioned the
  loop" — a runtime but AFFINE bound (`i < n`) is fine; a data-dependent EXIT
  CONDITION (`a[i] > limit`) is not.
- **CORRECT REASONING**: the vectorizer needs to know how many vector iterations
  to execute. With an affine bound it computes `n/4`; with a per-iteration
  condition it cannot without evaluating the condition (which is the loop).
  Reformulate as a full-range loop with a per-element guard when semantics
  allow.
- **EXAMPLE** (bad): `while (i < n && a[i] > limit) { a[i] = -a[i]; ++i; }` —
  never vectorized (OBSERVED: `missed: no stmts to vectorize`).
- **COUNTEREXAMPLE** (good): `for (i = 0; i < n; ++i) if (a[i] > limit)
  a[i] = -a[i];` — vectorized with `-O2 -mavx2` (OBSERVED: `32 byte vectors
  and unroll factor 8`); at `-O2`/`-O3` on plain SSE2 it stays scalar
  (`missed: not vectorized: unsupported control flow in loop`).
- **VERIFICATION**: `examples/bad/loop.c` `bad_unknown_trip` vs
  `examples/good/loop.c` `good_affine_trip`.
- **SOURCE**: `gcc-manual` (vectorizer, trip-count analysis).

## 6. `-O2` vs `-O3`: the cost model, not the pass

- **RULE**: the tree vectorizer (`-ftree-vectorize`) is enabled at `-O2`, but
  the default vector cost model at `-O2` is `very-cheap`; `-O3` switches it to
  `dynamic`. Many profitable loops vectorize at `-O3` but not at `-O2` — a
  policy decision, not a missing pass.
- **WHY AI GETS IT WRONG**: "`-ftree-vectorize` is on at `-O2`, so `-O2` and
  `-O3` vectorize identically" — the pass runs at both levels, but the cost
  model rejects loops at `-O2`.
- **CORRECT REASONING**: `-fvect-cost-model=very-cheap|cheap|dynamic`; default
  at `-O2` is very-cheap, at `-O3` dynamic (per GCC manual). Override with
  `-O2 -fvect-cost-model=dynamic` when you need `-O2`-level codegen with
  aggressive vectorization.
- **EXAMPLE** (bad): `c[i] = b[i] * 2;` with `restrict` — OBSERVED not
  vectorized at `-O2` (scalar `leal`), vectorized at `-O3`.
- **COUNTEREXAMPLE** (good): `c[i] = b[i] + 1;` vectorizes at `-O2`
  (OBSERVED); `-O2 -fvect-cost-model=dynamic` also vectorizes `*2`, the
  runtime-add `+x`, and a pure copy loop.
- **VERIFICATION**: `gcc -O2 -fopt-info-vec=.. ` vs `gcc -O3 -fopt-info-vec=..`
  on `examples/good/loop.c` `good_mul`/`good_reduction`; OBSERVED `-O3` adds
  `optimized: epilogue loop vectorized using 8 byte vectors and unroll factor 2`.
- **SOURCE**: `gcc-manual` (`-fvect-cost-model`, `-ftree-vectorize` under
  Optimize Options); `agner-fog` (why the model matters in practice).

## 7. Alignment: legal on x86-64, not "required"

- **RULE**: unaligned SIMD loads/stores (`movdqu`/`movups`) are baseline on all
  SSE2+ x86-64. Misalignment is a code-quality concern (cacheline/page
  boundaries), not a legality blocker, on modern x86-64. GCC 16.1 emits
  unaligned moves even for `_Alignas(64)` data. On targets without unaligned
  SIMD access (pre-SSE2 x86, older NEON) alignment is a hard requirement.
- **WHY AI GETS IT WRONG**: "vectorization requires 16-byte alignment" is a
  legacy SSE/NEON rule; agents apply it to x86-64 and misreport unaligned
  buffers as non-vectorizable.
- **CORRECT REASONING**: check the target ISA first. On x86-64, alignment
  affects instruction choice and boundary-crossing cost, not whether the loop
  vectorizes. `_Alignas`/`aligned_alloc` matter for (a) intrinsics that require
  aligned operands (`_mm_load_si128`), (b) cacheline/cross-page behavior, (c)
  targets without unaligned access.
- **EXAMPLE** (bad): `c[i + 1] = b[i] * 2;` (store 4 bytes off) — OBSERVED not
  vectorized at `-O2`; at `-O3` vectorized with the store at `+4` bytes:
  `movups %xmm0, 4(%rcx,%rax)`.
- **COUNTEREXAMPLE** (good): aligned store `c[i] = b[i] * 2;` — OBSERVED also
  scalar at `-O2` (cost model, not alignment); at `-O3` vectorized. The asm
  still uses `movdqu`/`movups`; `_Alignas(64)` globals (G2) and
  `__builtin_assume_aligned(p,16)` (G6) did NOT produce `movdqa` on this
  toolchain.
- **VERIFICATION**: grep `loop.s` for `movdqa|movdqu|movups`. OBSERVED good
  file at `-O2`: `movdqu (%rcx,%rax), %xmm0` + `paddd` + `movups %xmm0,
  (%rdx,%rax)`; no `movdqa`.
- **SOURCE**: `intel-sdm` Vol.2 (`movdqu`, `movups` semantics); `intel-opt-manual`
  (alignment and memory optimization); `iso-c11-n1570` §7.22.3.1
  (`aligned_alloc`), §6.7.5 (`_Alignas`); `gcc-manual`
  (`__builtin_assume_aligned`); `arm-sve-acle` (unaligned-vector AArch64).

## 8. Runtime dispatch: don't ship AVX2 in a baseline binary

- **RULE**: code compiled with `-mavx2` may execute AVX2 instructions anywhere;
  on a CPU without AVX2 it traps with Illegal instruction. To ship one binary,
  keep a baseline build and dispatch the AVX2/AVX-512 path behind
  `__builtin_cpu_init()` + `__builtin_cpu_supports("avx2")` (GCC) or
  `ifunc`/`__attribute__((target("avx2")))`.
- **WHY AI GETS IT WRONG**: "compiling with `-mavx2` auto-generates a fallback"
  — it does not; there is no automatic fallback.
- **CORRECT REASONING**: `-march`/`-mavx2` authorizes the compiler to emit
  VEX-encoded instructions (prefix `v`, ymm registers) anywhere in the
  translation unit. The function attribute form confines them to one function:
  `__attribute__((target("avx2"), noinline))` gated by a runtime check.
- **EXAMPLE** (bad): global `-mavx2` binary run on an AVX1-only CPU → `SIGILL`.
- **COUNTEREXAMPLE** (good): `if (__builtin_cpu_supports("avx2")) fast_avx2(...)
  else scalar(...);` with `fast_avx2` marked `target("avx2")`.
- **VERIFICATION**: compile with `-mavx2`, inspect asm for `v`-prefixed ymm
  instructions outside the gated function; run on hardware without AVX2.
- **SOURCE**: `gcc-manual` (Function Attributes `target`, Other Builtins
  `__builtin_cpu_supports`/`__builtin_cpu_init`); `intel-sdm` Vol.1 (VEX
  encoding); `agner-fog` (portability cost of ISA-specific code).

## 9. Intrinsics vs auto-vectorization

- **RULE**: prefer auto-vectorization for element-wise and reduction loops; the
  compiler handles unrolling, tail handling, and target evolution. Reach for
  intrinsics only when the operation is not expressible by the vectorizer:
  cross-lane shuffles, gathers/scatters, saturating arithmetic, masked ops, or
  when a fixed ISA guarantee is required.
- **WHY AI GETS IT WRONG**: "intrinsics are always faster than
  auto-vectorization" — for a basic add loop the generated code is identical;
  hand intrinsics freeze ISA assumptions, often omit unrolling/tail logic, and
  rot when the target moves to wider vectors.
- **CORRECT REASONING**: write the portable loop first, read the missed dump to
  identify the actual unsupported pattern, then replace ONLY that operation
  with the matching intrinsic (e.g. `_mm_shuffle_epi32`, `_mm_i32gather_epi32`).
  Keep a scalar fallback path. SVE ACLE intrinsics are width-agnostic; NEON
  intrinsics are fixed 128-bit — prefer SVE-style or portable code when width
  evolution matters.
- **EXAMPLE** (bad): replacing a trivially auto-vectorized `c[i]=a[i]+b[i]` loop
  with `_mm_add_epi32` intrinsics and hand-rolled tail code.
- **COUNTEREXAMPLE** (good): auto-vectorize the add loop; use
  `_mm_i32gather_epi32` only for the index-dependent access the vectorizer
  refuses.
- **VERIFICATION**: compare `gcc -O3 -fopt-info-vec` output and asm against the
  intrinsic version; they must match in width, and the intrinsic version must
  prove the extra complexity.
- **SOURCE**: `intel-intrinsics` (intrinsic semantics/availability);
  `intel-opt-manual`; `arm-sve-acle` (width-agnostic SVE vs fixed NEON);
  `agner-fog` (instruction tables for shuffles/gathers).

## 10. Reading vector asm: xmm/ymm registers

- **RULE**: register width is the vector width: `xmm` = 128 bit (4× int32),
  `ymm` = 256 bit (8× int32), `zmm` = 512 bit. `paddd` = packed int32 add;
  `pslld $1` = packed int32 shift left; `movdqu`/`movups` = unaligned 128-bit
  load/store; the `v` prefix (`vpaddd`) means AVX (VEX-encoded, 256-bit).
- **WHY AI GETS IT WRONG**: "a `paddd` instruction proves SIMD was used for the
  loop" — a lone xmm instruction (e.g. `movss` for a float copy, `pxor` in
  unrelated code) is not loop vectorization; also "`ymm` = AVX-512" is wrong
  (AVX-512 uses `zmm`).
- **CORRECT REASONING**: confirm three things in sequence: (a) the loop counter
  advances by the vector width (e.g. `addq $16, %rax` for 4× int32), (b) a
  vector load → vector arithmetic → vector store inside the loop, (c) the
  scalar epilogue handles the `n % width` tail.
- **EXAMPLE** (bad): agent claims vectorization from `movdqu` alone in a
  function with no loop.
- **COUNTEREXAMPLE** (good): identify `movdqu (%rcx,%rax), %xmm0; paddd
  %xmm1, %xmm0; movups %xmm0, (%rdx,%rax); addq $16, %rax; cmpq %r9, %rax;
  jne` as a 4-wide vectorized int32 loop with a scalar epilogue.
- **VERIFICATION**: OBSERVED good file at `-O2`: `pcmpeqd %xmm1,%xmm1`,
  `psrld $31, %xmm1` (builds the constant 1), `movdqu` load, `paddd`, `movups`
  store, `addq $16, %rax` (16-byte step); bad file at `-O2`: zero xmm/ymm
  instructions.
- **SOURCE**: `intel-sdm` Vol.1 (SIMD/FP registers), Vol.2 (instruction set);
  `agner-fog` (instruction tables).

## 11. `restrict` is a contract, not a hint

- **RULE**: violating `restrict` is UB (C11 §6.7.3.1): the compiler may assume
  disjoint objects and vectorize/optimize on that basis. Wrong results are the
  consequence, not slower code.
- **WHY AI GETS IT WRONG**: "`restrict` is a performance hint, harmless if
  wrong" — it is a semantic promise the optimizer is licensed to exploit.
- **CORRECT REASONING**: add `restrict` only where the caller truly passes
  disjoint buffers; document the requirement. `__builtin_assume_aligned` proves
  alignment, NOT non-aliasing — OBSERVED: `good_assume_aligned` at `-O3` still
  emitted `optimized: loop versioned for vectorization because of possible
  aliasing` because its parameters lack `restrict`.
- **EXAMPLE** (bad): `memcpy`-style overlap passed to a `restrict`-annotated
  function → vectorized wrong results.
- **COUNTEREXAMPLE** (good): `restrict` on `b` and `c` in `good_alias` with the
  caller guaranteeing disjoint arrays; runtime self-check passes (OBSERVED
  `RUN_CORRECTNESS` builds print `OK`, exit 0).
- **VERIFICATION**: run `gcc -O2 -DRUN_CORRECTNESS examples/good/loop.c` and
  the `bad` twin; both must print `OK` (vectorized output equals the scalar
  reference).
- **SOURCE**: `iso-c11-n1570` §6.7.3.1 (restrict contract), §6.5p7
  (aliasing); `gcc-manual` (strict aliasing).

## Quick decision table

| Symptom in the missed dump | Likely cause | Fix |
|---|---|---|
| `possible dependence between data-refs` | aliasing or serial chain | add `restrict`; or restructure |
| `no stmts to vectorize` | blocker analysis gave up (alias at `-O2`, trip count, pattern) | read `-fopt-info-vec-all`, isolate the loop |
| `unsupported control flow in loop` | `if`/break inside loop | if-convertible guard or `-O2 -mavx2` |
| loop silent at `-O2`, vectorized at `-O3` | cost model (`very-cheap`) | `-fvect-cost-model=dynamic` or accept `-O3` |
| `movdqu` everywhere | normal on modern x86-64 | not a bug; alignment is a niche concern |
| `SIGILL` on old CPU | `-mavx2` in baseline binary | runtime dispatch |
