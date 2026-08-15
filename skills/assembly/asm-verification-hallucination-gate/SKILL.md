---
name: asm-verification-hallucination-gate
description: Use when an agent produces or reviews assembly and claims an instruction or encoding is valid. Gate every mnemonic, operand, width, and stack offset through assemble + disassemble + byte compare against an ISA manual, because generated mnemonics are often invented. Prevents fabricated instructions, AT&T reversals, and byte-blind claims.
---

# Assembly Verification Hallucination Gate

## When to use

- The task involves generating, fixing, or reviewing hand-written assembly
  (any syntax, any ISA: x86-64, Thumb-2, NEON, RISC-V).
- An agent asserts "this is a valid instruction", "AX is 8 bits", or "these
  bytes mean X" and a mnemonic-level correctness decision is on the table.
- Reviewing an AI-authored assembler PR, bootloader, or inline asm for
  hallucinated mnemonics, wrong operands, or bad encodings.

## When not to use

- Reading/writing compiler-generated assembly you are not changing — use
  `asm-optimizer-artifacts` instead.
- Stack layout / calling conventions in detail — use `asm-calling-conventions`.
- Syntax-dialect rules (NASM `$`, size hints, `default rel`) — use
  `asm-syntax-dialects-nasm-gas-att`.
- The code is already verified end-to-end by re-execution and byte round-trip.

## What the agent often gets wrong

- Emits invented mnemonics (`movqad`) and foreign pseudo-ops (`JOB`, `SST`,
  `OCT` from CDC COMPASS) that no assembler accepts.
- Reverses AT&T operands (`movl 0x0, -0x4(%rbp)` for "store 0 to [rbp-4]") and
  forgets `$` on immediates; silent swaps like `movq %rax,%rbx` assembling fine
  while doing the opposite of the intent.
- Claims "AX is 8-bit"; AX is 16, AL/AH are 8.
- Wrong stack offsets (`8(%rsp)` where `(%rsp)` is correct) that assemble
  cleanly and silently corrupt data.
- Hand-writes or asserts encodings without disassembling: `69 c0 00 00 00 00`
  claimed as `imul eax,eax,38` decodes to `imul $0x0,%eax,%eax`; `8b 00`
  claimed as `mov (%r8),%eax` is really `mov (%rax),%eax`.
- Trusts "high confidence" instead of tool output — baselines are 44%/36%
  IR→asm correctness and 14% disassembler exact match.

## How to reason correctly

1. **Mnemonic check**: for every mnemonic, locate it in the ISA manual
   (intel-sdm, arm-arm, nasm-manual). Unfindable = invented = reject.
2. **Syntax check**: fix the dialect first (AT&T source-first and `$`, or
   NASM/Intel) before evaluating semantics.
3. **The gate**: `gcc -c` (or target assembler), then `objdump -d` (or target
   disassembler), then compare the emitted bytes with what you intended. If
   mnemonic or operands differ, the encoding is wrong.
4. **Width and offset audit**: name the register width per line (AL/AH 8, AX
   16, EAX 32, RAX 64) and walk pushes/pops to fix stack offsets.
5. **Runtime check**: where execution is possible, run the snippet on edge
   inputs (0, -1, 0xFFFFFFFF) and compare results with the intent.
6. **Calibrate**: without a machine gate, treat any assembly claim as coin-flip
   (44% x86_64 baseline) and label it UNVERIFIED.

## What to verify

- Every mnemonic exists in the manual and assembles (exit 0) or is rejected
  (exit 1) exactly as predicted.
- `objdump -d` shows the intended bytes: `6b c0 26` for `imul eax,eax,38`, not
  `69 c0 00 00 00 00`; `48 89 d8` for `mov %rbx,%rax`.
- Register widths are correct per line; no "AX is 8-bit" class errors.
- Stack offsets match the current stack pointer state; runtime test passes.
- The file's bad/ examples behave as labeled: assembler rejects or silently
  wrong as documented.

## How to verify

```
gcc -c bad/invented_mnemonic.s     # exit 1: "no such instruction"
gcc -c bad/att_inverted.s          # exit 1: "operand type mismatch"
gcc -c good/gate_pass.s            # exit 0
objdump -d good/gate_pass.o        # 6b c0 26 | 89 d8 | c7 45 fc 00 00 00 00
objdump -d bad/imul_nulled_imm.o   # 69 c0 00 00 00 00 -> imul $0x0
objdump -d bad/byte_blind.o        # 8b 00 -> mov (%rax),%eax
```

Runtime checks (stack offset, silent swap) are pre-linked programs; record
their exit codes. For other targets (Thumb-2, NEON, RISC-V) substitute the
target assembler + `-march` flags — the gate procedure is identical.

## Where the knowledge comes from

- `intel-sdm` Vol.2 (instruction set, REX, IMUL forms), Vol.1 (registers,
  stack).
- `amd64-apm` Vol.1/Vol.3 (REX, register widths, IMUL).
- `binutils-docs` (GNU as syntax, objdump); `nasm-manual` (Intel dialect).
- `sysv-amd64-abi` (stack/arg layout, Windows x64 differs).
- Calibration: `arxiv-2511-01183` (NeuComBack 44%/36% IR→asm),
  `arxiv-2505-11480` (SuperCoder 51.5%→95%), `arxiv-2407-02524` (14% exact
  match disassembler), `arxiv-2605-29059` (SCDBench 7%).
- Failure survey `research/2026-08-15-asm-agent-failures-survey.md` (ASM-1..21).

## Related skills

- `asm-syntax-dialects-nasm-gas-att` — dialect rules this gate presupposes.
- `asm-x86-64-registers-and-addressing` — widths/encodings the gate verifies.
- `asm-calling-conventions`, `asm-inline-asm-constraints` — runtime context.
- `binary-analysis-type-recovery` — decompilation fidelity (adjacent problem).

## Evaluation

- Synthetic: `examples/bad/` must be flagged — invented mnemonic and CDC
  pseudo-ops rejected by the assembler; silent cases (swap, AX width, stack
  offset, nulled immediate, byte-blind) caught by review or disassembly.
- False-positive: `examples/good/` must NOT be flagged — valid AT&T, correct
  `$` immediate, correct widths, correct offsets, correct IMUL imm8 encoding.
- Historical: HerraduraKEx PR#33 (wrong stack slot), BBoeOS PR#584 (nulled
  IMUL immediate) — both reproduced as `examples/bad/`.
- Adversarial: `bad/att_silent_swap.s` assembles cleanly and runs, but does
  the opposite of the documented intent.
- Commands and recorded output: `evals/README.md`.
