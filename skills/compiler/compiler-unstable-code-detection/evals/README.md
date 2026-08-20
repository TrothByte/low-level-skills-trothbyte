# Evaluation — compiler-unstable-code-detection

Skill: `skills/compiler/compiler-unstable-code-detection`. Stability target: `evaluated`.

## Verified facts (host, recorded 2026-08-20)

Host: Windows, gcc 16.1.0 (MinGW, "Rev5, Built by MSYS2 project"), Python 3.11.9.
clang NOT installed — LLVM commands below are target documentation.

Toolchain record:

```
gcc.exe (Rev5, Built by MSYS2 project) 16.1.0
```

Runner: `python examples/tools/diff_test.py` (compiles with `-Wall -Wextra -Werror`,
runs each binary, compares stdout + exit code). Real output, 2026-08-20:

`examples/bad/signed_overflow_o2.c` — signed overflow check `y = x + 1; return y > x;`
with `x == INT_MAX` (UB, C11 6.5p5). Diverges:

```
  -O0: rc=0 stdout='no-overflow'
  -O1: rc=1 stdout='overflow-detected'
  -O2: rc=1 stdout='overflow-detected'
  -O3: rc=1 stdout='overflow-detected'
  RESULT: DIVERGENCE
```

`examples/bad/strict_aliasing_o2.c` — `my_function(int *a, float *b)` writes and
re-reads the same storage through both pointer types (UB, C11 6.5p7). Diverges:

```
  -O0: rc=0 stdout='r=1075838976'
  -O1: rc=0 stdout='r=1075838976'
  -O2: rc=0 stdout='r=1'
  -O3: rc=0 stdout='r=1'
  RESULT: DIVERGENCE
```

`examples/good/deterministic.c` — well-defined unsigned factorial, PASS marker.
Stable:

```
  -O0: rc=0 stdout='PASS factorial(12)=479001600'
  -O1: rc=0 stdout='PASS factorial(12)=479001600'
  -O2: rc=0 stdout='PASS factorial(12)=479001600'
  -O3: rc=0 stdout='PASS factorial(12)=479001600'
  RESULT: stable
```

All three files compile clean under `-Wall -Wextra -Wpedantic -Werror` at -O0 and -O2.
Honest note: simple single-function punning (local/global float written through an
`unsigned int *`) does NOT diverge on gcc 16.1 MinGW; the function-boundary pattern
above is what actually splits TBAA here. Union type punning is handled as defined on
this host. The overflow fold appears at -O1 (frontend/RTL constant folding), the
aliasing split only at -O2 (inlining) — divergence boundaries are version-specific.

## Synthetic evals

- **easy/negative**: `int f(int x){ return x+1 > x; }` — must answer: signed overflow
  UB; comparison folds to 1 at -O1+; verify with `diff_test.py` + `-O2 -S`.
- **easy/negative**: `int x = *p; if (!p) ...` — deleted null check (UB: deref before
  null check); must name `compiler-ub-assumptions` reasoning, prove via `-S`.
- **medium/negative**: `*a = 1; *b = 2.5f; return *a;` with aliased storage — must
  detect strict aliasing, minimize to the function boundary, confirm with
  `-fno-strict-aliasing` flip (behavior must return to `r=1075838976`).
- **medium/positive**: unsigned wrap check `x + 1 > x` — must NOT report UB; well-defined.
- **hard/negative**: uninitialized local read whose output changes between -O0/-O2 —
  must identify uninitialized variable, not guess stack layout.
- **ambiguous**: empty `for(;;){}` — must NOT assert "always removed"; must note
  GCC vs Clang/C++ divergence and the optimizer license on loop termination.

## False-positive evals

- Well-defined code with no UB — must NOT claim miscompilation when `-O0` == `-O2`.
- An `unsigned` wrap check that keeps its real semantics — must NOT flag as UB.
- A `volatile`-flagged spin loop — must NOT flag as elided.
- A program that differs only in wall-clock time — must NOT be reported as a divergence
  (observable behavior = stdout + exit code, not timing).
- A struct-padding or `sizeof`-layout difference (implementation-defined) — must be
  classified as implementation-defined, not UB.

## Historical evals

- **Classic -O2 null-check elimination**: Linux kernel TUN driver `tun_chr_poll()`
  dereferenced `tun` before the `if (!tun)` check; gcc optimized the null check away,
  enabling a local privilege escalation (documented in LWN "Fun with NULL pointers,
  part 1", July 2009, https://lwn.net/Articles/342330/). Kernel mitigation:
  `-fno-delete-null-pointer-checks`. Agent must explain why gcc removes the check and
  why the fix is reordering the check, not disabling optimization.
- **CVE-2021-* class**: compiler-optimization-dependent flaws (null-check/bounds-check
  elimination changing security behavior) are recorded in CVE databases; the agent
  must not invent specific CVE numbers — any CVE-2021 claim must be verified against
  NVD before being asserted, and the reasoning must show the optimization mechanism
  regardless of the CVE ID. (KNOWN: the class exists; specific IDs: UNVERIFIED here.)

## Adversarial evals

- Present a wrapper program that compiles clean under `-Wall -Wextra -Wpedantic -Werror`,
  passes UBSan/ASan runs, and still diverges at -O2. The agent must not conclude
  "no UB" from clean sanitizer output; it must run the differential matrix.
- Present a program with two independent UB sites and demand a minimal reproducer —
  agent must minimize until only one site remains.
- Ask "is this a compiler bug?" for a divergent program — agent must first prove UB
  (or its absence) before entertaining a compiler-bug claim.
- Give a `-fwrapv` "fix" and ask whether it is sufficient — must answer: masks only
  overflow, leaves aliasing/shift/other UB; never a permanent fix.

## Verification commands (target)

Host (gcc 16.1 MinGW, clang absent):

```
python examples/tools/diff_test.py examples/bad/*.c examples/good/*.c
gcc -Wall -Wextra -Wpedantic -Werror -O0 examples/bad/signed_overflow_o2.c -o b0 && ./b0
gcc -Wall -Wextra -Wpedantic -Werror -O2 examples/bad/signed_overflow_o2.c -o b2 && ./b2
gcc -O2 -S examples/bad/signed_overflow_o2.c -o -    # folded compare
```

Target (clang/LLVM, not installed on this host):

```
clang -O0 a.c -o a0 && ./a0
clang -O2 a.c -o a2 && ./a2
clang -O2 -S file.c
clang -fsanitize=undefined file.c && ./a.out
```

## Scoring

- detection: runs the differential matrix and correctly reports a divergence where
  the code is unstable (stdout + exit code, not timing).
- reasoning: names the UB class from `c-undefined-behavior` and predicts the
  optimizer transform BEFORE compiling.
- minimization: reduces to the minimal trigger and isolates a single UB site.
- confirmation: flips `-fwrapv`/`-fno-strict-aliasing` or UBSan to pin the site, and
  proves the transform in generated asm.
- fix: removes UB at the source; does NOT ship `-fwrapv`/`-fno-strict-aliasing`.
- honesty: records toolchain version with every result and documents cases that do
  NOT diverge on a given compiler version.
