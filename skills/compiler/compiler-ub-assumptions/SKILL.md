---
name: compiler-ub-assumptions
description: Use when diagnosing why C/C++ code behaves differently across optimization levels or compilers, when a bounds/null check "disappears" at -O2, when the optimizer reorders or elides code, or when explaining assumption-based optimization. Teaches how compilers exploit undefined behavior and how to prove the behavior with disassembly.
---

# How the Compiler Interprets Undefined Behavior

## When to use

- Code works at `-O0` but breaks at `-O2`/`-O3`.
- A runtime check (bounds, null) is "skipped" — the optimizer deleted it.
- A loop becomes infinite, an `if` becomes constant-true/false, at higher optimization.
- Reviewing whether generated asm matches the C source intent.

## When not to use

- Explaining implementation-defined behavior (padding layout, `>>` on negative signed) — different rules.
- Actual compiler bugs — first rule out UB. See `c-undefined-behavior` for the taxonomy.

## What the agent often gets wrong

- "The compiler is buggy" — the code contains UB and the compiler is allowed to assume it never happens.
- "The check is still there because I wrote it" — if the UB implies the check is dead, it is removed.
- "volatile will fix it" — `volatile` does not make UB defined; it only forces re-reading/writing.
- "It worked on my machine at -O0, so the code is correct" — `-O0` keeps naive codegen; `-O2` applies the UB assumptions.

## How to reason correctly

1. Identify the UB (use `c-undefined-behavior` taxonomy).
2. Ask: "what can the optimizer assume if this UB never happens?" That assumption is the exploit.
3. Predict the transformation: signed overflow → compare folded; deref before null check → null check deleted; OOB → loop bound removed.
4. Prove it: compile with `-S -O2` and read the asm (or use Godbolt).
5. Fix the UB at the source — never by disabling optimization globally.

## What to verify

- `-O2` asm matches intent: checks present, no surprise constant folding.
- Removing UB makes the asm sane again.
- The same UB reproduces the same assumption across GCC and Clang (both implement the standard the same way).

## How to verify

```
gcc -O2 -S file.c            # read generated asm
gcc -O0 -S file.c            # compare against -O0
# for a specific transform, isolate it into one function and diff the asm
```

## Where the knowledge comes from

- ISO C11 N1570 §6.5p5 (UB), Annex J.2; C++20 [intro.abstract]
- Chandler Carruth, "Garbage In, Garbage Out" (CppCon 2016)
- Matt Godbolt, "What Has My Compiler Done for Me Lately?"
- GCC/Clang optimization documentation

## Related skills

- `c-undefined-behavior` — the UB taxonomy (require of)
- `c-integer-promotion-and-conversion` — the integer-specific UB
- `asm-optimizer-artifacts` — reading optimizer output when analyzing

## Evaluation

Adversarial: signed-overflow loop that becomes infinite at -O2; deleted null check;
OOB-dependent check removal; comparison folded to constant. The agent must identify the UB,
predict the transform, prove it in asm, and fix the source.
