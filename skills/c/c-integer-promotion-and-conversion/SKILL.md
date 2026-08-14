---
name: c-integer-promotion-and-conversion
description: Use when writing or reviewing C arithmetic where signed/unsigned mixing, integer promotion, narrowing, or size_t vs int conversions can cause wrong results or overflow — comparisons, array sizes, allocation sizes, and length calculations. Teaches the usual arithmetic conversions and the classic wrap surprises.
---

# C Integer Promotions & Conversions

## When to use

- Writing arithmetic mixing `int`, `unsigned`, `size_t`, `char`, `short`, or literals.
- Reviewing comparisons between signed and unsigned values.
- Calculating buffer/allocation sizes from potentially large inputs (CVE-2022-0185, CVE-2016-8617).
- Diagnosing "works on 64-bit, breaks on 32-bit" (size_t narrowing).

## When not to use

- Pure unsigned-only arithmetic with no narrowing (still check overflow — see `c-undefined-behavior`).
- Floating point conversion (different rules — `FLP`).
- C++ — use `cpp-object-lifecycle` for C++-specific integer issues (ES.100-107).

## What the agent often gets wrong

- "`-1 < 1u` is true." The `-1` is converted to `unsigned`, becoming a huge value; the comparison is false.
- "`char`/`short` stay as-is in arithmetic." They promote to `int` (or `unsigned int` if `int` cannot hold them).
- "`size_t` and `int` compare fine." Negative `int` converts to a huge `size_t`; a negative sentinel never triggers.
- "`a * b` stays in `a`'s type." Both operands convert to a common type first; the product may overflow the common type silently.

## How to reason correctly

1. Apply integer promotion (types with rank < `int` → `int`/`unsigned int`).
2. Apply the usual arithmetic conversions (find the common type, signed → unsigned on tie).
3. Trace the value through conversion: signed → unsigned is NOT value-preserving.
4. Check overflow at the common type, before assignment/narrowing.

## What to verify

- Signed/unsigned comparisons behave as intended under `-Wsign-compare -Wconversion`.
- No silent narrowing to `int` of a `size_t` length (32-bit hazard).
- Multiplication before allocation cannot overflow (CVE-2016-8617 pattern).

## How to verify

```
gcc -Wall -Wextra -Wconversion -Wsign-conversion program.c
clang -fsanitize=undefined -fno-sanitize-recover=undefined program.c
```

## Where the knowledge comes from

- ISO C11 N1570 §6.3.1.1 (integer promotion), §6.3.1.8 (usual arithmetic conversions)
- SEI CERT C: INT30-C, INT31-C, INT32-C
- CVE-2022-0185, CVE-2021-33909, CVE-2016-8617

## Related skills

- `c-undefined-behavior` — overflow and shift UB (require)
- `compiler-ub-assumptions` — how overflow UB becomes optimizer assumptions

## Evaluation

Historical CVE: CVE-2022-0185 (unsigned arithmetic underflow), CVE-2016-8617 (alloc overflow),
CVE-2021-33909 (size_t→int truncation). Synthetic: signed/unsigned comparison surprise,
promotion of char/short. False-positive: correct unsigned-only wrap arithmetic must not be flagged.
