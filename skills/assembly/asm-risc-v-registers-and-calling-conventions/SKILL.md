---
name: asm-risc-v-registers-and-calling-conventions
description: Use when writing or reviewing RISC-V assembly — recursion, stack frames, and register roles. Covers callee-saved s0-s11 vs caller-saved a0-a7/t0-t6, RV64 8-byte slots and 16-byte alignment, leaf functions, and argument passing. Prevents the recorded garbage-sum failures from uninitialized s0 and 4-byte frames.
---

# RISC-V Registers and Calling Conventions

## When to use

- Writing or reviewing RISC-V assembly (RV32/RV64) — functions, recursion,
  stack frames, register allocation.
- Debugging garbage return values in recursive or multi-call functions.
- Porting code that uses s0/s1/... or ra across calls and getting corruption.
- Checking ABI compliance before calling libc/compiler-generated code.

## When not to use

- x86-64 calling conventions (SysV/Windows) — use `asm-calling-conventions`.
- RISC-V instruction-set semantics beyond registers (memory model, PMP) —
  those live in `riscv-isa-spec`-backed kernel skills.
- Verifying that an instruction exists/encodes correctly — use
  `asm-verification-hallucination-gate`.
- RISC-V vector (RVV) ABI — that is `arm-sve-acle`-style RVV territory.

## What the agent often gets wrong

- Uninitialized `s0` in recursion: the function "saves" s0 but never
  establishes its own value, then reads garbage after the recursive call
  (recorded failure: sum = garbage).
- 4-byte frame instead of 8 for (s0 + ra) on RV64: misaligned sp and
  overlapping slots.
- Treating callee-saved `s0-s11` as scratch: clobbering the caller's value.
- Over-saving in leaf functions (or forgetting that a non-leaf must save
  `ra`).
- Using `sw`/`lw` (RV32 4-byte) in RV64 code where `sd`/`ld` is required.
- Reading args from the stack when they were passed in `a0-a7`.

## How to reason correctly

1. Classify every register you touch: caller-saved (`a0-a7`, `t0-t6`), or
   callee-saved (`s0-s11`). If your function makes a call, save `ra` and any
   s-register you use, and restore them before `ret`.
2. If the function is a leaf (no calls), skip the prologue entirely — only a/t
   registers, no frame.
3. Size the frame on RV64 as slots × 8, rounded to 16-byte sp alignment:
   s0+ra → `-16` with `sd s0,0(sp)`, `sd ra,8(sp)`.
4. For recursion: each level writes its own frame; the restore must read the
   frame this prologue wrote, before `ret`.
5. Match arg/return registers to the psABI (a0-a7 in, a0/a1 out); spill only
   beyond 8 args.
6. Keep prologue/epilogue symmetric (`-N`/`+N`, mirrored `sd`/`ld`).

## What to verify

- Function compiles/assembles (researched:
  `clang --target=riscv64-unknown-elf -march=rv64gc -c` /
  `riscv64-linux-gnu-gcc`).
- Every s-register saved and restored; no scratch use of s0-s11.
- `ra` saved in every non-leaf; leaves have no prologue.
- RV64 frames: `sd`/`ld`, 8-byte slots, 16-byte sp alignment.
- Recursion restores from the current frame; result matches a scalar sum.

## How to verify

```
# Researched — no RISC-V toolchain on this host. Verification commands:
clang --target=riscv64-unknown-elf -march=rv64gc -S examples/good/recursion_s0.s
clang --target=riscv64-unknown-elf -march=rv64gc -c examples/good/recursion_s0.s
clang --target=riscv64-unknown-elf -march=rv64gc -c examples/bad/recursion_s0.s
riscv64-linux-gnu-gcc -c examples/good/callee_saved.s
```

Runtime check on a RISC-V host/emulator: `sum(5)` must equal 0+1+2+3+4+5;
bad frames return garbage.

## Where the knowledge comes from

- `riscv-psabi` — RV64 register roles, argument passing, stack layout,
  16-byte alignment.
- `riscv-isa-spec` — RV64I instructions (ld/sd, call/ret, addi).
- `sysv-amd64-abi` — contrast reference (different register classes).
- Failure survey `research/2026-08-15-asm-agent-failures-survey.md` (ASM-7:
  s0 uninitialized, 4-byte frame, garbage sum).

## Related skills

- `asm-calling-conventions` — x86-64 counterpart (different table).
- `asm-verification-hallucination-gate` — verify mnemonics before trusting.
- `abi-layout-reasoning` — C-level ABI layout reasoning.
- `asm-syntax-dialects-nasm-gas-att` — GAS syntax conventions apply here too.

## Evaluation

- Synthetic: bad RISC-V (uninitialized s0, 4-byte frame, s0 as scratch) must
  be flagged; good RISC-V (proper s0/ra save, 16-byte frame, leaf with no
  prologue, caller-saved-only helper) must pass.
- False-positive: leaf functions with no prologue and correct a-register
  usage must NOT be flagged as "missing save".
- Historical: the ASM-7 garbage-sum failure reproduced as `bad/recursion_s0.s`
  and `bad/frame_size.s`.
- Adversarial: `bad/recursion_s0.s` and `bad/frame_size.s` assemble and run,
  returning wrong sums — the "compiles fine, wrong result" class.
- Commands: `evals/README.md`. All cases are researched (no RISC-V toolchain
  here); no run claims are made.
