# IEEE 754 reference — what actually happens on the machine

Companion to `../SKILL.md`. Every claim below is either primary-source backed
(IEEE 754-2019, GCC docs, C11, Goldberg) or marked `[KNOWN]`/`[INFERRED]`/
`[UNVERIFIED]` per repository convention. Host observations were recorded on
GCC 16.1 (MSYS2 MinGW), x86-64 Windows.

## 1. Binary formats

- binary32 (C `float`), binary64 (C `double`), binary128; plus decimal64/128.
  Sign bit + biased exponent + significand. `double` = 53-bit significand
  (52 stored), 11 exponent bits. `float` = 24-bit significand (23 stored),
  8 exponent bits. `[KNOWN]`
- On x86 (32- and 64-bit MinGW/GCC/Clang) `long double` is the x87 **binary80
  extended** format: 64-bit significand, 15 exponent bits. On MSVC `long
  double` is identical to `double` (binary64). `[KNOWN]` — check `sizeof(long
  double)` per toolchain; never assume one answer.

## 2. Exactness: 0.1 is not representable

- `0.1` and `0.2` in decimal are infinite binary fractions; each literal
  rounds to the nearest binary64. `0.1 + 0.2` rounds once more, and the
  stored result is `0.30000000000000004`, not `0.3`. `[KNOWN]` — observed.
- Consequence: `==` on results of decimal arithmetic is almost always wrong.
  Equality is only meaningful for values that came from the same exact source
  (e.g. a bit-copied value, an integer converted exactly, `0.5`, powers of 2).
- `%a` prints exact hexadecimal representation; use it to see true values.

## 3. NaN and Inf

- NaN (Not a Number) and Inf are encoded in the exponent field. NaN has two
  payload kinds: quiet and signaling. `[KNOWN]`
- NaN compares unordered: `NaN < x`, `NaN == x`, `NaN > x` are all false.
  Therefore `x == x` is **false** for NaN and true otherwise — the canonical
  NaN detection idiom, also `isnan(x)`. `x == NAN` is always false. `[KNOWN]`
- NaN propagates: `f(x) = NaN` for most arithmetic and math functions on NaN
  input. Sign of a NaN result is not specified. `[KNOWN]` (IEEE 754-2019
  6.2).
- `0.0/0.0` → NaN; `x/0.0` → `±Inf`; `Inf-Inf`, `0*Inf` → NaN. FP division by
  zero is a well-defined result, NOT a crash (unlike integer division). `[KNOWN]`

## 4. Signed zero

- `-0.0` and `+0.0` compare equal (`-0.0 == 0.0` is true), but they are
  distinct bit patterns, and `1.0/x` gives `-Inf` for `x = -0.0`, `+Inf` for
  `x = +0.0`. `[KNOWN]` — observed. `-fno-signed-zeros` (implied by
  `-ffast-math`) destroys this distinction.

## 5. Rounding

- Default rounding mode: round-to-nearest, ties-to-even. Exceptions: toward
  zero, toward +Inf, toward -Inf (C `fenv.h` `FE_TONEAREST` etc.). `[KNOWN]`
- `fegetround()`/`fesetround()` read/write the mode; results of arithmetic
  change accordingly. `#pragma STDC FENV_ACCESS ON` is the standard's way to
  tell the compiler the environment is consulted. GCC historically ignores
  the pragma; use `-frounding-math` to make rounding-mode changes honored.
  `[KNOWN]` (GCC docs).
- The compiler is allowed to reassociate/contract only under flags. Default
  `-O2` preserves IEEE ordering for plain `a+b+c` evaluation. `[KNOWN]`

## 6. FMA contraction

- Fused multiply-add `fma(a,b,c)` computes `a*b+c` with ONE rounding. GCC
  fuses `a*b+c` under `-ffp-contract=fast` (implied by `-ffast-math`) when
  the target has FMA instructions (`-mfma` on x86-64). `[KNOWN]` — observed:
  a `vfmadd132sd` is emitted and the result differs from separate
  `vmulsd;vaddsd` by 1 ulp.
- Default for C is `-ffp-contract=off` unless `-ffast-math` is given.
  `[KNOWN]` — observed (no FMA in default asm).
- Contracting changes observable results; pin `-ffp-contract=off` for
  bit-reproducible builds. `[KNOWN]`

## 7. x87 excess precision

- On x87 (`-mfpmath=387`, 32-bit x86 default historically), `double`
  arithmetic happens in 80-bit registers and is rounded to 64-bit only when
  stored to memory. `[KNOWN]` (Goldberg sec. "Excess Precision"; GCC docs for
  `-ffloat-store`).
- Same source, different result depending on register allocation and
  optimization: a value kept in a register has more precision than a stored
  one. `[INFERRED]` — observed on this host: `0.1+0.2+0.3` = `0.60000000000000009`
  on SSE, `0.59999999999999998` with `-mfpmath=387`. A volatile-store
  register-vs-stored comparison was codegen-dependent (observed both equal
  and DIFFERENT).
- `-ffloat-store` forces every store to round to the declared type — the
  flag-level fix. `[KNOWN]` (GCC docs).
- x86-64 default uses SSE: binary64 ops are single-rounded; the x87 effect
  requires `-mfpmath=387` or 32-bit code. `[KNOWN]`

## 8. -ffast-math is a family of assumptions

`-ffast-math` implies `-fno-math-errno`, `-funsafe-math-optimizations`,
`-ffinite-math-only`, `-fno-trapping-math`, `-fno-signed-zeros`,
`-fno-rounding-math`, `-fassociative-math`, `-freciprocal-math`, and
`-ffp-contract=fast`. `[KNOWN]` (GCC Optimize-Options). Concretely it means:
assume no NaN, no Inf, no errno from math functions, no dependence on
rounding mode, and free reassociation. Each of these individually changes
observable behavior. `[KNOWN]` — observed: `sqrt(-1.0)` stops setting
`errno=EDOM`, and `(1e308*10)/10` stops overflowing (reassociated to
`1e308*(10/10)`).

## 9. C standard context

- C11 5.2.4.2.2 gives the model for float/double/long double characteristics
  (`FLT_*`, `DBL_*`, `LDBL_*` macros). Annex F makes `#pragma STDC IEC 60559
 `— when implemented — bind the implementation to IEC 60559 (IEEE 754)
  behavior. `[KNOWN]`
- `math_errhandling` tells whether math functions report errors via `errno`
  and/or exceptions (`MATH_ERRNO`, `MATH_ERREXCEPT`). `[KNOWN]`
- `FLT_EVAL_METHOD` (0, 1, or 2) tells whether intermediates are evaluated at
  a wider precision than the declared type — directly describes the x87
  situation (`FLT_EVAL_METHOD=2` for 80-bit). `[KNOWN]`

## 10. Comparison strategy

- Do not compare FP results with `==`; do not fix it with a magic `1e-6`.
  Derive a tolerance from the computation's error bound (forward error
  analysis), or compare exact integer quantities instead (convert to integer
  scaled units). `[KNOWN]` (Goldberg).
- A relative/magnitude-scaled tolerance (`fabs(a-b) <= max(1,|a|,|b|)*tol`)
  avoids the trap of absolute epsilons near zero and large values. `[KNOWN]`

## 11. float vs double

- `float` arithmetic is rounded at each step (not a synonym for "double with
  less range"). `printf("%f")` converts a `float` to `double` for varargs;
  `printf("%lf")` is a `double` in C (no-op on most implementations). `[KNOWN]`
- `f` suffix matters: `float f = 0.1;` converts the double literal to float at
  assignment (double rounding), `float f = 0.1f;` rounds once. `[KNOWN]` —
  observed: `float(0.1)` prints `0.100000001`.
- Do not use `float`/`double` for money, IDs, or timestamps; use integer or
  fixed-point. `[KNOWN]`

## 12. Numerical algorithms

- Naive summation of many floats accumulates error proportional to n; Kahan
  summation or pairwise/blocked reduction reduces it. `[KNOWN]` (Goldberg).
- Summing in a loop is not associative under default IEEE semantics; changing
  the reduction order (vectorization, `-ffast-math`) legitimately changes the
  result within rounding error. `[KNOWN]`

## 13. Historical failures (eval material)

- Patriot missile (1991): time in tenths of a second accumulated in a 24-bit
  register; after ~100 hours the accumulated error (~0.34 s) caused a
  tracking failure. `[KNOWN]` (public investigations).
- Ariane 5 (1996): a 64-bit float horizontal velocity converted to a 16-bit
  integer overflowed, raising an unhandled exception. `[KNOWN]`
- Pentium FDIV (1994): a missing entry in the radix-4 SRT division lookup
  table produced wrong results for specific operand patterns. `[KNOWN]`
- These are the canonical "FP failure classes": accumulation, conversion/
  overflow, and hardware/correctness mismatch. `[KNOWN]`

## 14. Verification recipes

- Build matrix: `-O0`, `-O2`, `-O2 -ffast-math`, `-O2 -ffp-contract=off`
  (and `-mfma` variants); diff outputs.
- ABI matrix: default SSE vs `-mfpmath=387`; cross-compile to ARM/AArch64 and
  compare bit patterns (`-march` matters for FMA there).
- Print `%a` to see exact values; `%Lf`/`%La` for `long double`.
- `fp_check.py` (this skill's `examples/tools/fp_check.py`) flags static
  anti-patterns: `==` on float literals, `x == NAN`, `-ffast-math` in build
  lines, unsuffixed `f` literals, float loop counters. It cannot detect
  excess precision, contraction, or NaN propagation — those require builds.
