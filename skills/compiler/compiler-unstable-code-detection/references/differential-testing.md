# Differential Testing for Unstable-Code Detection

## Idea

Differential testing runs *equivalent programs* under different implementations
and compares observable behavior. For compiler diagnostics the "implementations"
are:

- different compilers for the same language (gcc vs clang, gcc 12 vs gcc 16),
- different optimization levels of one compiler (`-O0/-O1/-O2/-O3/-Os`),
- different frontend settings of one compiler (`-std=c99` vs `-std=c23`),
- different targets of one compiler (x86-64 vs AArch64 via cross-compilers or
  compiler-explorer).

When the source is the same and the observable behavior (stdout, exit code,
memory writes) differs, one of two things holds:

1. the code has undefined behavior, and each configuration is free to do
   something different, or
2. a compiler miscompiled well-defined code (much rarer; rule out (1) first).

So differential testing is a *detector*: it converts "is this code well-defined?"
into "do two compilations agree?", which is decidable by running binaries.

## Why sanitizers alone are not enough

Sanitizers instrument specific UB classes at specific program points. They have
false negatives:

- `-fsanitize=undefined` misses strict-aliasing violations on some compilers and
  does not catch them when the bad access is optimized into a dead store before
  instrumentation runs.
- UBfuzz (ASPLOS 2024, arXiv 2401.04538) found 31 bugs in GCC and LLVM
  sanitizers by differentially comparing sanitizer output against oracle
  programs — i.e. sanitizers themselves disagree with each other and with the
  standard on real UB inputs.
- Undefined behavior can also be silent and value-changing rather than
  crashy/poison-like (e.g. a reordered load returns the "wrong" value without
  any fault), so a no-fault run is not a correctness proof.

The empirical rule used in this skill: a program is "clean" only when sanitizers
report nothing *and* behavior is identical across optimization levels. Either
signal alone is insufficient.

## Divergence classes and what provokes them

Usual suspects, in the order the agent should check them:

1. **Signed integer overflow** — `x + 1` where `x == INT_MAX`. Provocation:
   the comparison is folded at -O1 and above because the optimizer assumes the
   overflow cannot happen. On x86 the -O0 binary wraps in hardware; the -O2
   binary prints the "detected" branch.
2. **Strict-aliasing violation** — same storage accessed through two
   incompatible lvalue types (C11 6.5p7). Provocation: a function with
   `int *` and `float *` parameters writing and re-reading the same storage;
   TBAA then proves the accesses do not alias and the reload disappears.
3. **Shift counts** — `1 << 32` or `x << k` with `k` outside `[0, width-1]`.
4. **Uninitialized reads** — values depend on stack contents; optimizer may
   use a different value or none.
5. **Evaluation order** — `i = f() + g()` where the functions share mutable
   state; order is unspecified.
6. **Floating-point reassociation** — only under `-ffast-math`; a defined-mode
   build should not diverge.
7. **String functions** — `strlen`/`strcmp` called on non-NUL-terminated or
   overlong buffers.

## Minimization

Large reproducers mix many candidate UB sites. Apply ddmin-style source
bisection:

1. Record the divergent (compiler, level) pair, e.g. `(gcc, -O2)` vs `(gcc, -O0)`.
2. Delete or simplify parts of the source that do not affect the divergence
   (comment out statements, constant-fold inputs).
3. Keep any part whose removal makes the two runs agree again.
4. Iterate until the trigger is a handful of lines.

The minimized trigger makes the root cause obvious and is what should be filed
upstream or fixed.

## Confirm the root cause

- Toggle the UB in isolation:
  - `-fwrapv` should make the signed-overflow fold disappear (but it changes
    many programs, so use it only as a bisection instrument, never as the fix).
  - `-fno-strict-aliasing` should make the aliasing reload come back.
- Run UBSan/ASan on the minimized trigger: a report names the UB class directly.
- Read the generated asm (`gcc -O2 -S`) and confirm the exact transform
  (compare folded to a literal `mov $1, %eax; ret`).

## Worked verified example (this host, gcc 16.1 MinGW)

`examples/bad/signed_overflow_o2.c`:

```
gcc -O0  -> rc=0 stdout='no-overflow'
gcc -O1  -> rc=1 stdout='overflow-detected'
gcc -O2  -> rc=1 stdout='overflow-detected'
gcc -O3  -> rc=1 stdout='overflow-detected'
```

`examples/bad/strict_aliasing_o2.c` (function-boundary TBAA):

```
gcc -O0  -> rc=0 stdout='r=1075838976'
gcc -O1  -> rc=0 stdout='r=1075838976'
gcc -O2  -> rc=0 stdout='r=1'
gcc -O3  -> rc=0 stdout='r=1'
```

`examples/good/deterministic.c` (well-defined):

```
gcc -O0  -> rc=0 stdout='PASS factorial(12)=479001600'
gcc -O1  -> rc=0 stdout='PASS factorial(12)=479001600'
gcc -O2  -> rc=0 stdout='PASS factorial(12)=479001600'
gcc -O3  -> rc=0 stdout='PASS factorial(12)=479001600'
```

Note the asymmetry on this gcc: the overflow fold appears already at `-O1`,
while the aliasing reload disappears only at `-O2` (inlining threshold). The
divergence boundary is itself optimizer-version-specific — another reason to
record the exact toolchain (`gcc --version`) with every result.

## Target commands (clang / LLVM)

clang is not installed on this host; the following are the target verification
commands:

```
clang -O0 a.c -o a0 && ./a0
clang -O2 a.c -o a2 && ./a2
clang -O2 -S file.c -o -        # generated asm
clang -fsanitize=undefined file.c -o au && ./au
clang -fsanitize=address file.c -o aa && ./aa
clang -O2 -Xclang -fdiscard-value-names -S file.c
```

Compiler Explorer (https://godbolt.org): paste the source, add `-O0` and `-O2`
tabs, switch compiler from gcc to clang, and compare both the asm panels and
the run output.

## Tooling in this skill

- `examples/tools/diff_test.py` — compiles each source at `-O0..-O3` with gcc,
  runs the binaries, compares stdout + exit code, prints DIVERGENCE or stable.
- `examples/bad/*.c` — verified divergent programs (recorded 2026-08-20).
- `examples/good/deterministic.c` — verified stable program (PASS marker).

## Related research

- UBfuzz (arXiv 2401.04538): differential testing of sanitizers against oracle
  programs; 31 sanitizer bugs found in GCC/LLVM.
- DiffSpec (arXiv 2410.04249): differential testing with LLMs using natural
  language specs as the oracle for code generated from different compilers.
- DESIL (arXiv 2504.01379): detects silent bugs in the MLIR compiler stack by
  differential (bug-reproducing) compilation.
- CompDiff: differential testing of compilers for UB (research preprint;
  cross-check claims against the papers above and primary docs).
