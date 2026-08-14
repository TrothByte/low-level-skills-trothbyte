---
name: asm-inline-asm-constraints
description: Use when writing, reviewing, or porting inline assembly in C, C++, or Rust — GCC extended asm constraints, memory and register clobbers, asm goto, and Rust asm! operand classes. Teaches declaring every asm side effect so the optimizer cannot miscompile around it.
---

# Inline Assembly Constraints & Clobbers

## When to use

- Writing or reviewing `__asm__` / `asm!` that touches registers or memory.
- Debugging "works at `-O0`, wrong value at `-O2`" around inline asm.
- Porting a GCC asm template to Rust or between compilers.
- Implementing atomics, barriers, or CPU instructions (cpuid, mfence, rdtsc).

## When not to use

- C is sufficient (compiler intrinsics exist for most common ops).
- A full assembly file (`.S`) with the same hardcoded registers — constraints
  only apply to inline asm.
- Rust safe code — `asm!` is `unsafe`; prefer intrinsics/atomics there too.
- Debugging a non-asm issue that merely shows up in generated asm.

## What the agent often gets wrong

- "My asm only touches the pointer I passed, no clobber needed." Without
  `"memory"`, the compiler treats the asm as memory-inert and reorders/merges
  surrounding accesses (verified: a store pair collapses, a double read
  collapses, a runtime result changes from 3 to 2).
- "I don't need to list the register — I write it in the template." The compiler
  does not parse templates; it allocates live values into any unlisted register.
- "`i` accepts any int." `"i"` is an immediate constant only; a runtime value is
  "impossible constraint".
- "An output is declared with `r`." Outputs require `=` or `+`.
- "Rust asm! is like GCC asm." Rust uses Intel syntax (destination first);
  AT&T templates silently compute the wrong value (verified: returns `a` instead
  of `a+b`).
- `volatile` alone is a full compiler barrier (it is not; add `"memory"`).

## How to reason correctly

1. List every effect of the template: which registers are written, which memory
   is touched, which flags change.
2. Declare each effect: outputs (`=r`/`+r`), inputs, clobbers (`"rax"`, `"cc"`,
   `"memory"`). If memory is touched, add `"memory"` or use a `"m"` operand.
3. Pick constraints that match the instruction's fixed operands (`"c"` for shift
   counts, `"a"`/`"A"` for multiply, `"i"` only for constants).
4. Check instruction-order hazards: use matching `"0"`/`"N"` or early-clobber
   `&` so inputs are read before outputs are written.
5. For Rust, write Intel syntax and pick operand classes (`out`, `in`, `inout`,
   `lateout`, `const`, `options(nostack)`) for the same hazards.

## What to verify

- Compiles clean: `gcc -Wall -Wextra -Werror -O2`.
- `-O2 -S` output keeps the memory/register accesses you expect (no merged
  stores, no cached loads, no missing clobber spill).
- Runtime result is correct with a non-constant input, not just inlined
  constants.
- Rust: no `asm_sub_register` warning; result asserted, not assumed.

## How to verify

```
gcc -Wall -Wextra -Werror -O2 -S examples/bad/missing_memory_clobber.c -o bad.s
gcc -Wall -Wextra -Werror -O2 -S examples/good/clobbers.c -o good.s
diff bad.s good.s                 # count loads/stores around each asm
gcc -Wall -Wextra -Werror -O2 examples/bad/missing_memory_clobber.c && run
rustc --edition 2021 -O examples/good/rust_asm.rs && ./rust_asm
```

## Where the knowledge comes from

- `gcc-manual` — Extended Asm, Constraints, Clobbers, asm goto, operand modifiers
- `clang-docs` — Inline Assembly compatibility notes
- `rust-reference` — inline assembly chapter (Intel syntax, operand classes)
- `intel-sdm` — instruction encodings (shift count in cl, mul operand regs)
- `sysv-amd64-abi` — register classification, callee-saved registers

## Related skills

- `asm-x86-64-registers-and-addressing` — register set and addressing modes the
  templates rely on (require)
- `asm-optimizer-artifacts` — reading generated asm to confirm these guarantees
- `compiler-ub-assumptions` — why the optimizer exploits what asm leaves undeclared
- `memory-ordering-reasoning` — when the asm is a barrier or an atomic

## Evaluation

Synthetic: missing `"memory"` clobber reordering (store merge, load CSE), missing
register clobber, `"i"` with runtime value, `+`/matching-constraint conflict.
False-positive: a correctly constrained asm (operands, clobbers, memory) must
NOT be flagged. Adversarial: AT&T template in Rust `asm!` (silent wrong value),
commutativity assumption on `sub`, asm goto label misuse. Verified with GCC 16.1
and rustc 1.97.1 in `evals/README.md`.
