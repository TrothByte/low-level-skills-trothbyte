# Compiler UB Assumptions — Reference

Source: ISO C11 N1570 §6.5p5 (UB), Annex J.2; C++20 [intro.abstract] p5; Carruth GIGO; Godbolt talks.
Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.

## 1. Signed overflow → comparison folding

- **RULE**: signed overflow is UB (§6.5p5). The compiler assumes `x + 1 > x` is always true
  for signed `x` and folds the comparison to `1`.
- **WHY AI GETS IT WRONG**: "two's complement wraps, so `x + 1 > x` is false at INT_MAX."
- **CORRECT REASONING**: the standard makes overflow impossible to reason about; the optimizer
  picks any result, here the always-true one.
- **EXAMPLE** (bad): `int f(int x) { return x + 1 > x; }` at `-O2` → `mov eax, 1; ret`.
- **COUNTEREXAMPLE** (good): `unsigned f(unsigned x) { return x + 1 > x; }` keeps a real wrap test.
- **VERIFICATION**: `gcc -O2 -S file.c` — observe constant folding.
- **SOURCE**: N1570 §6.5p5; Carruth GIGO demo.

## 2. Null deref → null check deletion

- **RULE**: dereferencing a null pointer is UB (§6.5.3.2p4). The compiler assumes the pointer
  is non-null after a dereference, and deletes a later `if (!p)` because it is provably false.
- **WHY AI GETS IT WRONG**: "I check for null, the check is obviously needed."
- **CORRECT REASONING**: `int x = *p; if (!p) ...` — the dereference already proved `p != NULL`;
  the check is dead code and is removed.
- **EXAMPLE** (bad): see `examples/bad/ub_assumptions.c` `check_after_deref`.
- **COUNTEREXAMPLE** (good): `if (!p) return; int x = *p;` — check first.
- **VERIFICATION**: `gcc -O2 -S` — the check disappears; `-fno-delete-null-pointer-checks` keeps it.
- **SOURCE**: N1570 §6.5.3.2p4; CERT EXP34-C.

## 3. OOB / UB-in-loop → loop transformation

- **RULE**: if a loop body only runs on a condition the optimizer can prove false under
  non-UB assumptions, the loop may be removed or transformed. Out-of-bounds access inside a
  loop gives the optimizer license to assume bounds hold and strip checks/bounds logic.
- **WHY AI GETS IT WRONG**: "the bounds check protects the buffer, the optimizer keeps it."
- **CORRECT REASONING**: UB-based branch predicates are treated as never-taken; the check the
  agent relied on is removed, and the vulnerable access remains — silently.
- **EXAMPLE** (bad): a loop with an in-bounds guarantee that actually overflows; the optimizer
  assumes the guarantee and deletes defensive code.
- **COUNTEREXAMPLE** (good): write the loop so the bound is part of the well-defined contract
  (`i < n` with verified `n`).
- **VERIFICATION**: ASan + `-O2` (catches what the optimizer keeps); compare `-O0` vs `-O2`.
- **SOURCE**: N1570 §6.5p5; Godbolt talk on optimizer-visible UB.

## 4. Division by zero → divisor assumed nonzero

- **RULE**: `x / 0` is UB (§6.5.5p5). The optimizer is licensed to assume the divisor is
  nonzero and may reorder or delete a divisor-origin check.
- **WHY AI GETS IT WRONG**: "division by zero raises SIGFPE, so it's 'caught'."
- **CORRECT REASONING**: integer division by zero is UB, not a defined trap. Observed GCC 16
  behavior: the guard `if (y == 0)` is HOISTED to before the division (merged with the divide),
  so the check still executes — but the license to delete it exists, and other patterns (the
  divide result consumed unconditionally, guard later) allow the optimizer to delete it. The
  lesson: never rely on a check that sits AFTER a UB-inducing operation; order matters.
- **EXAMPLE** (bad): `int r = x / y; if (y == 0) return -1;` — semantics depend on optimizer choice.
- **COUNTEREXAMPLE** (good): `if (y == 0) return -1; int r = x / y;` — guard first.
- **VERIFICATION**: `gcc -O2 -S` — observe where the `test` lands relative to `idiv`.
- **SOURCE**: N1570 §6.5.5p5.

## 5. Empty infinite loop → assumed termination (compiler-divergent)

- **RULE**: C11 §6.8.5p6: an iteration whose controlling expression is not constant, that
  performs no I/O, does not access volatile objects, and does not touch atomics, MAY be
  assumed to terminate. C++11 [intro.abstract]: an infinite loop without observable behavior
  is UB.
- **WHY AI GETS IT WRONG**: "a `while(1);` spin loop is obviously fine."
- **CORRECT REASONING**: the license differs per language AND per compiler. Verified with
  GCC 16.1: both `-O0` and `-O2` KEEP `for(;;){}` as `jmp .L8` in both C and C++ mode, and a
  `main` calling `spin()` becomes the loop itself (inlined, "noreturn"). Clang exploits the
  C++ UB and ELIDES such loops. So "the loop disappears" is compiler-dependent — always
  verify empirically rather than asserting.
- **EXAMPLE** (bad): `for (;;) { }` with no observable effect — Clang C++ removes it; GCC keeps it.
- **COUNTEREXAMPLE** (good): use a `volatile` flag or real work — well-defined everywhere.
- **VERIFICATION**: `g++ -O2 -S` vs `clang++ -O2 -S` — compare.
- **SOURCE**: N1570 §6.8.5p6; C++20 [intro.abstract]p5; empirical GCC 16.1 / Clang.

## 6. Strict aliasing → reordering / wrong-code

- **RULE**: accessing an object through an incompatible lvalue type is UB (§6.5p7). The
  compiler may reorder or cache accesses, because different types "cannot alias."
- **WHY AI GETS IT WRONG**: "reinterpreting through a cast is fine on x86."
- **CORRECT REASONING**: `float f = *(float*)&i;` where `i` is `int` violates strict aliasing;
  the compiler may not see the write through the other type.
- **EXAMPLE** (bad): type-punning via pointer cast then using both.
- **COUNTEREXAMPLE** (good): `memcpy(&f, &i, sizeof f)` (compiles to the same load/store) or a union.
- **VERIFICATION**: `-fstrict-aliasing -Wstrict-aliasing=2`; diff `-O2` asm with and without.
- **SOURCE**: N1570 §6.5p7; CERT EXP39-C.

## 7. Reading uninitialized values → branch both ways

- **RULE**: reading an uninitialized automatic variable is UB (§6.3.2.1p2); the value is
  indeterminate and may differ per read.
- **WHY AI GETS IT WRONG**: "it's just some garbage; my code handles both branches."
- **CORRECT REASONING**: the optimizer may choose a value that makes both branches dead or
  fold comparisons, because any value is permissible.
- **EXAMPLE** (bad): `int x; if (x) A(); else B();` — compiler may emit only one branch.
- **COUNTEREXAMPLE** (good): initialize `x` to a defined value.
- **VERIFICATION**: MSan; `-Wuninitialized`; diff asm.
- **SOURCE**: N1570 §6.3.2.1p2; CERT EXP33-C.

## Detection workflow

```
1. Find the suspicious behavior difference (-O0 vs -O2, GCC vs Clang).
2. Isolate into the smallest function that reproduces it.
3. gcc -O2 -S and -O0 -S; diff. Look for: folded compares, deleted checks, removed loops.
4. Classify the UB (c-undefined-behavior taxonomy).
5. Fix the source; confirm the asm now matches intent at -O2.
```

## Calibration

- Most "compiler bugs" reported by agents are UB assumptions. Estimate: >90% at -O2.
- NEVER respond to an apparent miscompilation by disabling optimization or adding volatile
  without first proving UB absence with sanitizers at -O2.
