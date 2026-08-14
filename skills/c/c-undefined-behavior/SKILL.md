---
name: c-undefined-behavior
description: Use when writing, reviewing, or debugging C code where undefined behavior (UB) may be present — signed overflow, out-of-bounds access, uninitialized reads, invalid pointers, shift/aliasing violations, or when a program behaves differently across optimization levels or compilers. Teaches the J.2 UB taxonomy and how to detect each class.
---

# C Undefined Behavior Taxonomy & Detection

## When to use

- Writing new C code that must be correct across compilers and optimization levels.
- Reviewing C code for UB that a compiler "happens" to accept.
- Diagnosing code that works on `-O0` but breaks on `-O2`, or on GCC but not Clang.
- Interpreting an UBSan report or explaining why a check disappeared from asm.

## When not to use

- Implementation-defined or unspecified behavior (those are NOT UB — different rules).
- C++ — use `cpp-object-lifecycle` / `cpp-move-semantics`.
- Rust — use `rust-unsafe-reasoning`.

## What the agent often gets wrong

- "It compiles and passes tests, so it's correct." UB is legal to compile; the optimizer
  may then assume it never happens and delete your checks (see `compiler-ub-assumptions`).
- "Signed overflow just wraps like unsigned." Signed overflow is UB; unsigned wraps.
- "Reading uninitialized memory gives garbage but is harmless." It is UB and can trap or
  produce any value, including a value the optimizer assumed impossible.
- "Shifting by the width is fine." Shift by negative or `>= width` is UB.
- Treating implementation-defined behavior (e.g. `>>` on negative signed, struct padding
  layout) as UB, or vice versa.

## How to reason correctly

1. Classify the construct against the J.2 list (see `references/ub-taxonomy.md`).
2. Decide whether the optimizer can exploit it (many UB classes only bite under `-O2`).
3. Remove the UB at the source; do NOT "fix" it with `volatile` or by disabling optimization.
4. Verify with UBSan + `-O2` + asm inspection, not just a debug run.

## What to verify

- UBSan is clean at `-O2` (not just `-O0`).
- No warning under `-Wall -Wextra` and clang-tidy.
- asm at `-O2` preserves the checks you expect (no deleted bounds/null checks).

## How to verify

```
clang -O2 -g -fsanitize=undefined -fno-sanitize-recover=undefined program.c -o prog
./prog                      # must not report UB
clang -O2 -S program.c -o - | less   # inspect assumptions
```

## Where the knowledge comes from

- ISO C11 N1570 §6.3.1.1, §6.5, §6.5.7, §7.21, §7.22.3, Annex J.2 (191 items)
- cppreference C behavior page
- SEI CERT C (EXP33-C, INT30-C, INT34-C, ARR30-C, STR30-C, MEM30-C)

## Related skills

- `compiler-ub-assumptions` — why the optimizer exploits UB (recommend)
- `c-integer-promotion-and-conversion` — integer-specific UB (require of)
- `safe-low-level-from-scratch` — positive writing path

## Evaluation

Historical CVE (off-by-one, OOB): CVE-2022-3602, CVE-2018-16890, CVE-2016-8617.
Synthetic + adversarial: signed-overflow loop, deleted null check, shift UB.
False-positive: correct signed arithmetic in a bounded range must NOT be flagged.
