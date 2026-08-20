# Evaluation — floating-point-ieee-semantics

Skill: `skills/c/floating-point-ieee-semantics`. Stability: `source-backed`
(host GCC 16.1 demonstrates every claim; see "Verified facts").

## Verified facts (host, recorded 2026-08-20)

Host: Windows x86-64, gcc 16.1.0 (MSYS2 MinGW Rev5), python 3.11.9. All
commands run with `gcc -Wall -Wextra -Werror -O2`.

### 0.1 + 0.2 != 0.3 — examples/bad/float_eq.c

```
$ gcc -Wall -Wextra -Werror -O2 examples/bad/float_eq.c -o float_eq.exe
$ ./float_eq.exe; echo exit=$LASTEXITCODE
FALSE: 0.1 + 0.2 == 0.3 (0.10000000000000001 + 0.20000000000000001 = 0.30000000000000004)
float(0.1) = 0.100000001
exit=1
```

### -ffast-math changes observable behavior — examples/bad/fast_math_breaks.c

Two real divergences on the SAME source: `sqrt(-1.0)` stops setting
`errno == EDOM`, and `(1e308*10)/10` stops overflowing (reassociation).

```
$ gcc -Wall -Wextra -Werror -O2 examples/bad/fast_math_breaks.c -o fmb.exe
$ ./fmb.exe
sqrt(-1.0): isnan=-1 errno==EDOM=1
(1e308*10)/10 = inf (IEEE overflow)

$ gcc -Wall -Wextra -Werror -O2 -ffast-math examples/bad/fast_math_breaks.c -o fmb_fm.exe
$ ./fmb_fm.exe
sqrt(-1.0): isnan=-1 errno==EDOM=0
(1e308*10)/10 = 1e+308 (reassociated, no overflow)
```

Honest note: `isnan(r)` was NOT optimized away by `-ffinite-math-only` on
this GCC (prints -1 in both builds). The observable divergences were errno
(1 -> 0) and reassociation (inf -> 1e+308).

### NaN detection, sign of zero, tolerance compare — examples/good/nan_detection.c

```
$ ./nan_detection.exe
x != x detects NaN: 1 (isnan: -1)
1.0/+0.0 = inf   1.0/-0.0 = -inf
x == NAN would be false: 0
close_enough(0.1+0.2, 0.3): 1
```

### x87 excess precision — examples/bad/x87_excess_precision.c

Same source, different result by FP-stack model: `0.1+0.2+0.3` with one final
store to double evaluates 80-bit (x87) or 64-bit per addition (SSE).

```
$ gcc -Wall -Wextra -Werror -O2 examples/bad/x87_excess_precision.c -o x87sse.exe
$ ./x87sse.exe
0.1+0.2+0.3 = 0.60000000000000009
$ gcc -Wall -Wextra -Werror -O2 -mfpmath=387 examples/bad/x87_excess_precision.c -o x87.exe
$ ./x87.exe
0.1+0.2+0.3 = 0.59999999999999998
```

Honest note: x86-64 defaults to SSE, so excess precision requires
`-mfpmath=387` (or a 32-bit x87 target). A volatile-store register-vs-memory
comparison was codegen-dependent on this host (observed both "equal" and
"DIFFERENT" across program shapes at `-O2 -mfpmath=387`); the single-store
expression `x+y+z` diverges deterministically.

### FMA contraction — examples/good/fma_contraction.c

`a*b+c` fused into one rounding step differs by 1 ulp from separate
multiply-add. Requires `-mfma` (the default x86-64 target has no FMA
instructions); `-ffast-math` implies `-ffp-contract=fast` (verified: a
`vfmadd132sd` is emitted under `-O2 -ffast-math -mfma`).

```
$ gcc -Wall -Wextra -Werror -O2 -mfma -ffp-contract=off examples/good/fma_contraction.c -o fma_off.exe
$ ./fma_off.exe
a*b+c = 0x1.fffffffffffe8p+0 = 1.9999999999999947
$ gcc -Wall -Wextra -Werror -O2 -mfma -ffp-contract=fast examples/good/fma_contraction.c -o fma_fast.exe
$ ./fma_fast.exe
a*b+c = 0x1.fffffffffffe9p+0 = 1.9999999999999949
```

### fp_check.py static scanner

```
$ python examples/tools/fp_check.py examples/good; echo exit=$LASTEXITCODE
0 issue(s) across 2 file(s)
exit=0
$ python examples/tools/fp_check.py examples/bad; echo exit=$LASTEXITCODE
examples\bad\fast_math_breaks.c:2: FAST_MATH: -ffast-math present (assumes no NaN/Inf/errno)
examples\bad\fast_math_breaks.c:5: FAST_MATH: -ffast-math present (assumes no NaN/Inf/errno)
examples\bad\float_eq.c:13: FP_EQ: exact ==/!= against a floating literal
examples\bad\float_eq.c:20: NOF_SUFFIX: float 'fv' assigned unsuffixed literal '0.1'
4 issue(s) across 3 file(s)
exit=1
```

Documented limit: `x87_excess_precision.c` is not statically detectable
(excess precision and contraction are build-time, not source-time); verify by
building both FP-stack variants.

## Synthetic evals

- easy/negative: `double s = a + b; if (s == c)` — must flag FP equality.
- easy/negative: `if (x == NAN)` — must flag NAN_CMP and suggest `x != x`.
- medium/negative: `float f = 0.1;` (no `f` suffix) — must flag NOF_SUFFIX.
- hard/negative: summation loop whose result changes between `-O0` and
  `-O2 -ffast-math` — must not be "fixed" by asserting equality; must verify
  reduction-order tolerance.
- positive: `x != x` NaN check — must NOT be flagged or "cleaned up".
- positive: `fabs(a-b) <= scale*1e-9` tolerance compare — must NOT flag.

## False-positive evals (correct code must not be flagged)

- `double z = a + b; if (isnan(z))` — correct NaN handling, no flag.
- `int cmp(double a, double b) { return memcmp(&a,&b,8)==0; }` — bit compare,
  deliberately exact; no flag (no FP literal).
- `0.5f` and `2.0f` literals with `f` suffix — exact values are fine; no flag.
- `x == 0.0` sign-of-zero checks on values that are exactly zero from
  integer conversion — borderline: flag only `==` with a non-zero FP literal.
- `long double` used where the ABI guarantees extended precision — not an
  anti-pattern by itself; no flag.

## Historical evals

- Patriot missile (1991): time-in-tenths accumulated in a 24-bit register;
  after ~100 h the accumulated error (~0.34 s) caused target tracking
  failure. Class: accumulation/precision.
  Fixture: a tenths counter incremented as `double` and compared with `==`.
- Ariane 5 (1996): 64-bit float horizontal velocity converted to a 16-bit
  integer overflowed; unhandled exception, rocket loss. Class: FP-to-integer
  conversion overflow. Fixture: `(int16_t)h_velocity` on `[0, 1e6]` inputs.
- Pentium FDIV (1994): radix-4 SRT division lookup table with 5 missing
  entries produced wrong quotients for specific operand patterns. Class:
  hardware/correctness mismatch. Fixture: verify results change when a
  software fallback rounds the same operands.
- Each eval: DETECT the FP assumption -> EXPLAIN the IEEE rule -> FIX
  (tolerance/integer/fixed-point, or pinned flags) -> VERIFY across builds.

## Adversarial evals

- Code that "fixes" float equality with a magic `1e-6` that is both
  too-loose near large values and too-tight near zero.
- A function whose NaN/Inf path is only reachable via untrusted input, where
  a reviewer deletes the `x != x` guard as "dead code".
- Code audited only at `-O0`; `-O2 -ffast-math` changes results silently
  (errno lost, reassociation hides overflow) while all tests pass.
- `-ffp-contract=fast` on a target WITH FMA vs without: reviewer must notice
  results differ by 1 ulp and pin the flag.
- ARM cross-compile that produces different bits for `0.1+0.2+0.3` than the
  x86 SSE build; reviewer must not "unify" by rewriting source.

## Verification commands (target — cross-compile/ARM, fenv testing)

```
# cross-architecture bit comparison (aarch64-linux-gnu-gcc or clang --target=aarch64)
aarch64-linux-gnu-gcc -Wall -Wextra -Werror -O2 examples/bad/x87_excess_precision.c -o /tmp/x87_arm
/tmp/x87_arm    # record; compare with SSE and x87 host runs

# fenv rounding-mode testing
cat > /tmp/fenv_t.c <<'EOF'
#include <fenv.h>
#include <stdio.h>
int main(void) {
    fesetround(FE_UPWARD);
    printf("%d\n", fegetround() == FE_UPWARD);
    return 0;
}
EOF
gcc -O2 -frounding-math /tmp/fenv_t.c -o /tmp/fenv_t.exe
/tmp/fenv_t.exe   # expect 1; without -frounding-math GCC may fold/misreport

# x87 legacy ABI (32-bit) for real excess precision
gcc -m32 -O2 -ffloat-store examples/bad/x87_excess_precision.c -o /tmp/x87_fs.exe

# reproducibility matrix (must all print same value when semantics pinned)
gcc -O0 examples/good/fma_contraction.c -o /tmp/f0.exe
gcc -O2 -ffp-contract=off examples/good/fma_contraction.c -o /tmp/f1.exe
/tmp/f0.exe; /tmp/f1.exe
```

## Scoring

- precision: every reported FP issue maps to a named IEEE-754/GCC flag class
  (representation, NaN, excess precision, contraction, fast-math).
- recall: each bad fixture must be caught (static via `fp_check.py`, dynamic
  via the build matrix).
- FP-rate: good fixtures must produce zero issues from `fp_check.py` and
  identical recorded outputs where semantics are pinned.
- calibration: agent must not propose `-ffast-math` or FMA contraction as a
  "free speedup" without an audited consequence list.
