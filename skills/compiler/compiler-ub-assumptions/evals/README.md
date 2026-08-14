# Evaluation — compiler-ub-assumptions

Skill: `skills/compiler/compiler-ub-assumptions`. Stability target: `evaluated`.

## Adversarial evals (core)

| ID | Case | Expected agent behavior |
|---|---|---|
| AD-02 | signed-overflow loop that becomes infinite / compare folded at `-O2` | identify signed overflow UB, predict constant folding, prove via `-O2 -S`, fix with unsigned |
| AD-03 | works `-O0`, fails `-O2` | find the UB, demonstrate the optimizer assumption in asm, fix at source |
| AD-09 | "the compiler deleted my null check" | explain deref-before-check pattern, prove deletion, reorder check first |
| AD-04 | works GCC fails Clang | if UB: explain optimizer license; if impl-defined: name the section |

## Synthetic evals

- **easy/negative**: `int f(int x){ return x+1 > x; }` — detect folding; expected answer: signed overflow UB.
- **medium/negative**: `int x = *p; if (!p) ...` — detect deleted null check.
- **hard/negative**: division guard after divide — detect fragile ordering, note the optimizer license.
- **ambiguous**: empty `for(;;){}` — must NOT assert "always removed"; must note GCC/Clang divergence (GCC 16 keeps, Clang C++ elides).

## False-positive evals

- Correct code with no UB — must NOT claim miscompilation.
- A valid `unsigned` wrap check — must NOT flag as UB.
- An empty spin loop on a `volatile` flag — must NOT flag.

## Verification fixtures

- `examples/bad/ub_assumptions.c` vs `examples/good/ub_assumptions.c`
- Commands:
  ```
  gcc -O0 -S examples/bad/ub_assumptions.c -o /tmp/o0.s
  gcc -O2 -S examples/bad/ub_assumptions.c -o /tmp/o2.s
  diff /tmp/o0.s /tmp/o2.s
  ```
  Confirmed with GCC 16.1.0: `check_after_overflow` → `movl $1, %eax; ret` (folded);
  `check_after_deref` → load+ret, null check deleted; `check_after_div` → guard hoisted before `idiv`;
  `spin` → kept as `jmp .L8` (GCC), elided by Clang in C++ mode.

## Scoring

- detection: correctly names the UB class from `c-undefined-behavior`.
- reasoning: correctly predicts the transform BEFORE running the compiler.
- fix: removes UB at source, does NOT disable optimization globally.
- verification: demonstrates the claim with asm, not assertion.
