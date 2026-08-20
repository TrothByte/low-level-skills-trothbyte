---
name: floating-point-ieee-semantics
description: Use when writing or reviewing floating-point code — NaN/Inf handling, -ffast-math effects, x87 80-bit excess precision, rounding, FMA contraction, or when FP results differ across optimization levels or platforms. Teaches IEEE 754 semantics LLMs assume wrong.
---

# Floating-point IEEE 754 semantics

## When to use

- Writing or reviewing code that computes with `float`, `double`, or
  `long double`: comparisons, summation, math functions, mixed types.
- Debugging why an FP program changes results at `-O2`, under `-ffast-math`,
  or when ported from x86 to ARM (or between compilers).
- Deciding whether `-ffast-math` (or `-ffp-contract=fast`, `-mfma`) is safe
  for a codebase, and auditing its consequences.
- Handling NaN/Inf in protocol, parsing, or sensor code where inputs are
  untrusted or arithmetic can diverge.
- Reviewing code that uses `==` on FP values, `x == NAN`, `float` loop
  counters, or `float` for money/IDs/timestamps.

## When not to use

- Exact integer, fixed-point, or decimal (money, IDs, timestamps) arithmetic —
  use integer types there, not FP at all.
- Bit-level layout details of an IEEE format (denormal thresholds, exponent
  extraction) — see `binary-analysis-type-recovery` or the format tables in
  `references/ieee754.md` as needed.
- Compiler/CPU memory-model effects unrelated to FP (threading, ordering) —
  see `memory-model-arm-x86-riscv`.
- Performance tuning of a numeric kernel where semantics are already
  understood and pinned — use `performance-measurement-discipline`.

## What the agent often gets wrong

- "Fixing" `0.1 + 0.2 != 0.3` with `if (fabs(a-b) < 1e-6)` — a magic constant
  tolerance that is wrong near zero and for large magnitudes. Equality on FP
  results is suspect to begin with; tolerate based on the computation's error
  bound.
- Proposing `-ffast-math` to "speed it up" without auditing NaN/Inf/errno
  assumptions. `-ffast-math` is a family: it assumes no NaN/Inf, no errno
  from math, no rounding-mode dependence, and reassociates expressions. It
  changes observable results (see `examples/bad/fast_math_breaks.c`).
- Assuming the same FP source yields identical bits on x86 SSE, x87, and ARM
  across `-O0`/`-O2`. Intermediate precision (`FLT_EVAL_METHOD`) and FMA
  availability differ per target and per flag.
- "Cleaning up" `if (x != x)` as dead code. `x != x` is the canonical NaN
  detection idiom (false for every normal value, true for NaN). Replacing it
  with `x == NAN` is always false and silently breaks the check.
- Assuming NaN never occurs or that NaN comparisons are ordered. `NaN < x`,
  `NaN == x` are all false; NaN propagates through most operations.
- Believing `long double` is always 80-bit (MSVC aliases it to `double`) or
  that `double == long double`.
- Not knowing FMA contraction (`a*b+c` fused into one rounding step) changes
  results by up to 1 ulp — see `examples/good/fma_contraction.c`.
- Using `float`/`double` for money, IDs, timestamps, or loop counters.
- Writing naive float accumulation and expecting exact sums; reduction order
  changes the result legitimately.
- Thinking FP division by zero crashes. It returns Inf/NaN; only integer
  division by zero is a fault.

## How to reason correctly

1. Decide determinism requirements first. If bit-identical results across
   platforms/optimizations are required, either avoid FP (fixed-point,
   integer) or pin the semantics: no `-ffast-math`, `-ffp-contract=off`,
   document `FLT_EVAL_METHOD`.
2. Compare with a tolerance derived from the computation's error bound
   (relative, magnitude-scaled), or compare exact integer quantities.
   Never a magic `1e-6`.
3. Detect NaN as `x != x` or `isnan(x)`; never `x == NAN`.
4. Audit `-ffast-math`: grep for NaN/Inf-dependent logic, errno-dependent
   math (`math_errhandling`), ordering-sensitive reduction; if unsure, drop
   `-ffast-math` and use `-ffp-contract=off` + targeted flags instead.
5. Choose types deliberately: `double` by default; `float` only for storage
   or measured performance with known precision needs (use `f` suffixes);
   `long double` only where the ABI actually guarantees extended precision
   (x87; not MSVC) and the cost is acceptable.
6. Verify FP code across `-O0`/`-O2`/`-ffast-math` AND across targets (SSE,
   x87 `-mfpmath=387`, ARM cross-compile); record any differences instead of
   assuming equivalence.

## What to verify

- No `==`/`!=` on FP results, except the deliberate `x != x` NaN idiom.
- NaN/Inf handled explicitly where inputs are untrusted or math can diverge
  (`sqrt`, `log`, divisions, `0.0/0.0`, overflow paths).
- `-ffast-math` absent unless audited; `-ffp-contract` choice documented
  (`off` for bit-reproducible builds).
- Same source produces results stable within tolerance across `-O0`/`-O2`;
  cross-target differences documented (SSE vs x87 vs ARM).
- Type choices justified: `f` suffixes on `float` literals, no `float` loop
  counters, no FP for money/IDs.
- `fenv` usage: `#pragma STDC FENV_ACCESS`, `fegetround`/`fesetround` honored
  (GCC needs `-frounding-math`).

## How to verify

```
python examples/tools/fp_check.py examples/good   # expect 0 issues, exit 0
python examples/tools/fp_check.py examples/bad    # expect flags, exit 1

gcc -Wall -Wextra -Werror -O2 examples/bad/float_eq.c -o float_eq.exe
./float_eq.exe            # FALSE: 0.1 + 0.2 == 0.3

gcc -Wall -Wextra -Werror -O2 examples/bad/fast_math_breaks.c -o fmb.exe
gcc -Wall -Wextra -Werror -O2 -ffast-math examples/bad/fast_math_breaks.c -o fmb_fm.exe
./fmb.exe; ./fmb_fm.exe   # outputs must differ (errno, reassociation)

gcc -Wall -Wextra -Werror -O2 examples/bad/x87_excess_precision.c -o x87sse.exe
gcc -Wall -Wextra -Werror -O2 -mfpmath=387 examples/bad/x87_excess_precision.c -o x87.exe
./x87sse.exe; ./x87.exe   # 0.60000000000000009 vs 0.59999999999999998

gcc -Wall -Wextra -Werror -O2 -mfma -ffp-contract=off examples/good/fma_contraction.c -o fma_off.exe
gcc -Wall -Wextra -Werror -O2 -mfma -ffp-contract=fast examples/good/fma_contraction.c -o fma_fast.exe
./fma_off.exe; ./fma_fast.exe   # differs by 1 ulp (vfmadd vs mul+add)
```

Print with `%a` (exact hex) instead of `%g` when diagnosing; check
`sizeof(long double)` per toolchain; consult `references/ieee754.md`.

## Where the knowledge comes from

- IEEE 754-2019 standard (https://ieeexplore.ieee.org/document/8766229, https://en.wikipedia.org/wiki/IEEE_754)
- GCC floating-point options — -ffast-math, -fno-math-errno, -ffp-contract (https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html)
- What Every Computer Scientist Should Know About Floating-Point Arithmetic (Goldberg 1991)
- C11 standard 5.2.4.2.2 / annex F — IEC 60559 support
- cppreference — floating point environment (https://en.cppreference.com/w/c/numeric/fenv)

## Related skills

- `compiler-ub-assumptions` — optimizer assumptions (incl. FP reassociation/FMA) and how to audit them
- `c-integer-promotion-and-conversion` — integer promotion rules adjacent to FP promotion (`f` suffix, `float`->`double`)
- `c-undefined-behavior` — FP is not UB in the same way, but conversion overflow and `fenv` misuse are defined-behavior traps
- `performance-measurement-discipline` — measure before proposing `-ffast-math`; verify the win with benchmarks
- `memory-model-arm-x86-riscv` — platform differences; same source, different FP results across ISAs
- `asm-optimizer-artifacts` — reading generated FP code (vfmadd, mulsd/addsd, x87) to confirm what the compiler did

## Evaluation

See `evals/README.md` for recorded host facts (2026-08-20), synthetic,
false-positive, historical, and adversarial eval fixtures, and verification
commands for ARM/fenv targets.
