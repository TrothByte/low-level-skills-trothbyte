---
name: asm-optimizer-artifacts
description: Use when reading compiler-generated assembly (gcc/clang -O2 -S, objdump, Godbolt) and explaining why machine code diverges from C source — inlining, tail calls, dead-code elimination, constant folding, lea strength reduction, RIP-relative addressing. Teaches spotting optimizer artifacts without misreading them as missing code.
---

# Optimizer Artifacts in Assembly

## When to use

- Reading `-O2`/`-O3` assembly produced from C/C++ and mapping it back to the source.
- Explaining why a function body "disappeared", a call became a `jmp`, or an `imul` became a `lea`.
- Checking whether generated code does what the source says (reviews, reverse engineering, ABI verification).
- Comparing `-O0` vs `-O2` disassembly to understand what the optimizer changed.

## When not to use

- Deciding whether source contains UB — use `c-undefined-behavior` / `compiler-ub-assumptions` first; optimizer behavior caused by UB is that skill's domain.
- Writing inline asm — use `asm-inline-asm-constraints`.
- Reading debug info to recover source variables — use `dwarf-debug-info`.
- Anything ARM/AArch64/RISC-V — these artifacts are x86-64 examples; the concepts transfer but the instructions differ.

## What the agent often gets wrong

- "The compiler deleted my function" — it was inlined; the body is now inside the caller.
- "The tail-call `jmp` means no call happened, so the program doesn't do what I wrote" — the call is still made, just without a new stack frame.
- "At -O0 the code is what I wrote, so -O2 asm is 'wrong'" — both are correct; `-O0` is the unoptimized (but still valid) form.
- "The `lea` must be a pointer operation" — `lea` is used for arithmetic, including `x*3`.
- "The `imul` disappeared, the multiply is gone" — it was strength-reduced to a cheaper instruction.
- "Dead variables must appear because I wrote them" — the optimizer removes results it can prove unused.
- "PIC means every access goes through GOT, so RIP-relative addressing is position-dependent junk" — RIP-relative is exactly how position-independence is achieved.

## How to reason correctly

1. Compile the SAME source at `-O0 -S` and at the target optimization `-O2 -S` and diff; a missing chunk in the `-O2` output is an optimization, not a bug.
2. For each function in the disassembly, ask "what source operation produces this instruction sequence?" before judging it missing.
3. Recognize the artifact signatures: no `call` where source calls (tail-call `jmp`), no `imul` where source multiplies by small constant (`lea`), no store where source assigns a dead local (DCE), no statements where source has compile-time constants (folding).
4. Verify semantics, not instruction count: an inlined body or a tail-call `jmp` produces the same observable result as the source.
5. Only after ruling out optimizer artifacts consider UB-based deletion (see `compiler-ub-assumptions`).

## What to verify

- The same source compiles at `-O0` and `-O2`; diff shows only the expected artifacts, not missing logic.
- A tail call: at `-O2` the callee receives the same arguments and the result is still returned.
- Inlining: the inlined function's statements are provably present (fused) in the caller body.
- Strength reduction: the constant multiplier result is computed correctly (e.g. `x*3` = `x+x+x`).
- DCE: the eliminated variable is truly unused on all paths.
- RIP-relative: the address is computed relative to RIP, not an absolute constant.

## How to verify

```
gcc -O0 -S source.c -o /tmp/o0.s
gcc -O2 -S source.c -o /tmp/o2.s
diff /tmp/o0.s /tmp/o2.s
```

Then read each function at both levels. For one artifact at a time, isolate the function into its own file. Use `objdump -d` on a linked binary when checking the final layout.

## Where the knowledge comes from

- GCC manual "Optimize Options" (`gcc-manual`)
- Intel SDM instruction set reference — `lea`, `jmp`/`call`, RIP-relative addressing (`intel-sdm`)
- System V AMD64 ABI — calling sequence, return (`sysv-amd64-abi`)
- Matt Godbolt, "What Has My Compiler Done for Me Lately?" (`godbolt-compiler`)
- Empirical GCC 16.1 (MinGW x86-64) output, recorded in `references/optimizer-artifacts.md`

## Related skills

- `asm-x86-64-registers-and-addressing` — register/addressing-model foundation (require of)
- `asm-calling-conventions` — why `call`/`ret` and args look the way they do
- `compiler-ub-assumptions` — when a missing check is UB, not an artifact (recommend of)
- `c-undefined-behavior` — UB taxonomy (recommend of)
- `binary-analysis-type-recovery` — uses this skill when reading optimized binaries

## Evaluation

Synthetic: tail-call `jmp`, `lea` strength reduction, inlined `caller`, DCE of a dead local, constant folding, RIP-relative global access. The agent must recognize each artifact and reconstruct the source semantics.
False-positive: unoptimized-looking `-O0` code must NOT be flagged as "wrong"; an absent `call` in a tail position must NOT be reported as "call lost"; a folded constant must NOT be called a compiler bug.
Adversarial: UB-driven deletion presented as an optimizer artifact — the agent must escalate to `c-undefined-behavior`.
