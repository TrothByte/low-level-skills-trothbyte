---
name: compiler-unstable-code-detection
description: Use when code behaves differently across compilers or optimization levels, when sanitizers report nothing but behavior changes at -O2, or when reviewing generated code for optimization-sensitive bugs. Teaches differential testing between compilers and optimization levels to find undefined behavior that sanitizers miss.
---

# Differential Testing for Optimization-Sensitive Bugs

## When to use

- The same source behaves differently at `-O0` vs `-O2`/`-O3`.
- Code behaves differently under gcc and clang (or across compiler versions).
- Sanitizers pass yet behavior still changes when optimization is enabled.
- Reviewing generated code for UB the optimizer exploits: elided bounds/null checks, folded comparisons, reordered loads and stores.
- Deciding whether a discrepancy is UB in the code or a genuine compiler bug.
- Backing a portability claim with evidence instead of "it compiled and ran".
- Example trigger: `int y = x + 1; if (y > x) ...` prints one branch at `-O0` and the other at `-O2`.

## When not to use

- Implementation-defined behavior (struct padding, signed right shift, `char` signedness) — different rules, not a divergence signal.
- Pure performance or timing differences with identical observable behavior.
- Floating-point reassociation under explicit `-ffast-math`, which the compiler is licensed to perform.
- Confirmed compiler bugs after UB has been ruled out on both compilers and both report the same behavior.
- Debugging a crash or memory error already pinned by a sanitizer — use the sanitizer report directly.
- Concurrency race artifacts that appear only under `-O2` — a race is not compiler
  instability; route to the concurrency skills instead.
- A documented, deliberate implementation choice (e.g. `-fno-strict-aliasing` in the
  build system) — the divergence is then explained by the flag, not by hidden UB.

## What the agent often gets wrong

1. Trusting one compiler at one `-O` level: "it compiled and ran" is treated as correctness.
2. Claiming code is portable without ever compiling with a second compiler or a second set of flags.
3. Blaming "optimizer bugs" instead of UB in their own code when `-O2` diverges from `-O0`.
4. Not minimizing: keeping huge reproducers that mix many potential UB sites.
5. Ignoring warning flags (`-Wall -Wextra -Wpedantic`) that would flag the UB at compile time.
6. Using `-fwrapv` / `-fno-strict-aliasing` as a permanent fix rather than a bisection tool.

## How to reason correctly

1. Behavior difference across compiler or `-O` level implies UB in the code until proven otherwise.
2. Minimize: reduce the program to the minimal trigger (ddmin-style bisection of the source).
3. Run the same source under `gcc -O0 -O1 -O2 -O3` and record exit code + stdout; any difference is a signal.
4. Check the usual suspects in order: signed integer overflow, strict aliasing, shift counts, uninitialized variables, evaluation-order assumptions, floating-point reassociation, string functions.
5. Cross-compile with clang when available (differential across compilers).
6. Confirm the root cause with `-fsanitize=undefined,address` or by toggling `-fwrapv`/`-fno-strict-aliasing`; behavior must flip at the UB site.
7. Fix the UB at the source. `-fwrapv` only masks one class and still leaves the rest.
8. Worked example: `check(INT_MAX)` with `int y = x + 1; return y > x;` — at `-O0` the
   hardware wraps and prints `no-overflow`; at `-O1+` the optimizer assumes the
   overflow cannot happen and folds the check, printing `overflow-detected`.
   A function-boundary `int *`/`float *` aliasing pattern instead flips only at `-O2`
   (inlining). The divergence boundary is itself optimizer-version-specific.

## What to verify

- Same source at multiple `-O` levels produces identical observable behavior (stdout and exit code).
- No warnings with `-Wall -Wextra -Wpedantic -Werror`.
- Sanitizers report nothing AND behavior is stable across levels — both, not either. Sanitizers have false negatives.
- Read the divergence matrix as a boundary report: record at which `-O` level behavior first changes and which compiler version produced it.
- Confirm the root cause by flipping one flag: `-fwrapv` must undo the overflow fold,
  `-fno-strict-aliasing` must bring the reload back — a flip that changes nothing means
  the suspected site is not the one causing the divergence.

## How to verify

```
python examples/tools/diff_test.py examples/bad/*.c examples/good/*.c
# expected: the two bad files DIVERGE; deterministic.c is stable.
# recorded 2026-08-20, gcc 16.1 MinGW:
#   signed_overflow_o2.c : -O0 'no-overflow' rc=0 ; -O1/-O2/-O3 'overflow-detected' rc=1
#   strict_aliasing_o2.c : -O0/-O1 'r=1075838976' ; -O2/-O3 'r=1'
#   deterministic.c      : all levels 'PASS factorial(12)=479001600' rc=0

gcc -Wall -Wextra -Wpedantic -Werror -O0 bad.c -o bad0 && ./bad0
gcc -Wall -Wextra -Wpedantic -Werror -O2 bad.c -o bad2 && ./bad2
# minimize the trigger with ddmin-style bisection before analyzing:
# comment out statements, keep only what makes the two runs disagree again.
# clang differential (target — clang not installed on this host):
clang -O0 a.c -o a0 && clang -O2 a.c -o a2 && ./a0 && ./a2
clang -O2 -S file.c           # read generated asm
clang -fsanitize=undefined file.c && ./a.out   # clang UBSan
# compiler-explorer: paste the source, add -O0 and -O2 tabs, compare asm + output
```

## Where the knowledge comes from

- UBfuzz: Finding Bugs in Sanitizer Implementations — ASPLOS 2024, arXiv 2401.04538 (https://arxiv.org/abs/2401.04538)
- DiffSpec: Differential Testing with LLMs using Natural Language Specifications — arXiv 2410.04249 (https://arxiv.org/abs/2410.04249)
- DESIL: Detecting Silent Bugs in MLIR Compiler Infrastructure — arXiv 2504.01379 (https://arxiv.org/abs/2504.01379)
- GCC documentation — optimization options (https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html)
- clang documentation (https://clang.llvm.org/docs/)
- CompDiff: differential testing of compilers for UB (research preprint, cross-check with the others above)

## Related skills

- `compiler-ub-assumptions` — the optimizer's UB assumptions that cause the divergence
- `c-undefined-behavior` — the UB taxonomy used to classify a found discrepancy
- `asm-optimizer-artifacts` — reading generated asm to confirm the transform
- `llvm-ir-reading` — clang IR when cross-compiling with LLVM (target)
- `sanitizer-agent-ci-loop` — running sanitizers in CI; know their false negatives
- `meta-verification` — verifying claims empirically rather than by assertion

## Evaluation

- `evals/README.md` records verified facts: real `-O0` vs `-O2` divergences reproduced on this host (gcc 16.1 MinGW), synthetic, false-positive, historical and adversarial cases, target verification commands, and scoring.
- Passing gate: `python tools/lint/skill_lint.py SKILL.md` reports 0 errors.
- The agent must run `diff_test.py` on the examples and explain any reported divergence before concluding the code is correct.
