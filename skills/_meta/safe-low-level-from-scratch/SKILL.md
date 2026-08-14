---
name: safe-low-level-from-scratch
description: Use when writing NEW low-level code (C/C++/Rust/asm) from scratch that must be memory-safe and correct across optimization levels and platforms. Provides the positive writing process integrating UB semantics, layout/alignment, ownership, atomics, and FFI, with verification gates at each step.
---

# Write Safe Low-Level Code From Scratch

## When to use

- Writing a new function/module in C/C++/Rust where memory safety matters.
- Any greenfield code touching buffers, pointers, threads, FFI, or hardware.
- Before the first compile: apply the process to avoid UB by design, not by fix.

## When not to use

- Auditing existing code — that is the `c-undefined-behavior` + `meta-rationalizations` flow.
- Pure algorithm work without memory/concurrency/FFI surface.
- Code already covered by a more specific skill (drivers → `kernel-uaccess-safety`, etc.).

## What the agent often gets wrong

- "It compiles, so it's correct" (B2) — writing first, verifying never.
- Choosing raw pointers/ownership models that make correctness impossible to argue.
- Mixing signed/unsigned and sizes without a plan (A2, A3).
- Using `Relaxed` ordering "because it's fastest" without a sync edge (A11).
- Verifying only at `-O0` (AD-03).

## How to reason correctly (the writing process)

1. **Define the contract**: inputs, invariants, ownership, thread-safety requirement.
2. **Choose types for safety**: RAII/descriptors (`cpp-raii-descriptor-types-api-design`),
   bounds-aware lengths (`size_t`), strong enum/typed errors.
3. **Design arithmetic**: decide signed/unsigned explicitly; compute sizes with checked
   multiplies; never compute a size then allocate without a check.
4. **Design memory ops**: choose `memmove` over `memcpy` when overlap is possible; terminate
   strings explicitly; verify every buffer index against a real bound.
5. **Design concurrency**: pick the sync edge first (Release/Acquire pair), then the ordering;
   relaxed only for counters.
6. **Design the boundary**: if FFI, fix layout (`repr(C)`/packed) and ownership transfer
   (who frees, who drops, who panics).
7. **Verify by design intent, then execute**: compile at `-O2` with warnings, run sanitizers,
   inspect asm where a claim matters.

## What to verify

- Ownership: every allocation has exactly one free/drop path (including error paths).
- Arithmetic: no unchecked multiplication before allocation; signed/unsigned decisions explicit.
- Memory: no uninit reads, no overlap without `memmove`, no off-by-one.
- Concurrency: every shared object is either immutable, atomic, or mutex-protected; sync edge exists.
- Boundary: layout matches the other side; no panic/unwind across FFI.
- Platform: assumptions listed and checked (ABI, endianness, widths).

## How to verify

```
# C/C++
gcc/clang -Wall -Wextra -Werror -O2 -g -fsanitize=address,undefined file.c -o prog
./prog                      # must run clean
# race check where threads exist
clang -fsanitize=thread     # or cargo +nightly miri for Rust
# asm check for UB-sensitive claims
gcc -O2 -S file.c
```

## Where the knowledge comes from

- Integrated from: `c-undefined-behavior`, `c-integer-promotion-and-conversion`,
  `memory-ordering-reasoning`, `abi-layout-reasoning`, `ffi-boundary-cross-language`.
- Primary: ISO C/C++ standards J.2/[intro.abstract]; CERT C; Rust Reference; SysV/AAPCS64.

## Related skills

- `c-undefined-behavior` (require), `memory-ordering-reasoning` (require),
  `abi-layout-reasoning` (recommend), `ffi-boundary-cross-language` (recommend),
  `meta-verification` (require), `meta-assumptions` (recommend).

## Evaluation

Synthetic: write a bounded ring buffer / a publish-consumer / an FFI wrapper from a spec —
the generated code must pass `-Werror + ASan/UBSan -O2` and the ownership/sync-edge audit.
Adversarial: spec with hidden UB traps (unbounded input, overlapping copy, misaligned FFI
field) — agent must design around them, not patch symptoms.
