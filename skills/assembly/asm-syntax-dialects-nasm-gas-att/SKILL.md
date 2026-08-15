---
name: asm-syntax-dialects-nasm-gas-att
description: Use when writing or reviewing assembly where the dialect matters — NASM Intel-style versus GNU as AT&T versus GNU as Intel mode. Covers operand order, immediates, size hints, label case, and default rel. Prevents silent address-vs-content bugs, reversed operands, and the four documented NASM error classes.
---

# Assembly Syntax Dialects: NASM / GAS / AT&T

## When to use

- Writing, porting, or reviewing assembly for NASM vs GNU `as` targets, where
  the same instruction has different textual spellings.
- Diagnosing "it assembles but does the wrong thing" that reduces to operand
  order, a missing `$`, a missing `[ ]`, or a missing size hint.
- Porting code between syntaxes (e.g. converting an AT&T snippet to NASM).
- Reviewing agent-generated assembly where dialect errors are common.

## When not to use

- Verifying whether an instruction exists or its encoding — use
  `asm-verification-hallucination-gate`.
- x86-64 register widths, flags, addressing modes — use
  `asm-x86-64-registers-and-addressing`.
- Calling conventions / argument layout — use `asm-calling-conventions`.
- Non-x86 ISAs (Thumb-2, NEON, RISC-V) have their own syntax.

## What the agent often gets wrong

- Mixes Intel operand order into AT&T (`movl 0x0, -0x4(%rbp)` for "store 0")
  and forgets `$` on immediates — `movl 5, %eax` loads from address 5.
- Forgets NASM size hints: `inc [counter]` fails with "operation size not
  specified"; GAS needs no hint because the suffix carries the size.
- Drops brackets: `mov rax, buf` (address) vs `mov rax, [buf]` (value) — the
  silent address-vs-content bug.
- Misses NASM label case-sensitivity: `Loop` ≠ `loop` ≠ `LOOP`, turning a
  branch into a jump to nowhere or the wrong loop.
- Omits `default rel` on Mach-O/PIE, leaving absolute relocations that the
  target rejects or mis-resolves.
- Uses `$` inside NASM 3.x directive arguments (`global $main`), valid only in
  2.x.

## How to reason correctly

1. Identify the toolchain and its default dialect: `gcc`/`as` = AT&T unless
   `-masm=intel` or `.intel_syntax noprefix`; `nasm` = Intel-style.
2. Apply that dialect's rules consistently: order, immediate marker, memory
   brackets, size hints, case, RIP-relative default.
3. For every memory operand in NASM ask: address or content? If content, use
   `[ ]`; if address, use `lea`.
4. For every size-ambiguous NASM operand, spell byte/word/dword/qword.
5. Convert using the table in references/syntax-dialects.md, then verify by
   assembling and comparing encodings with `objdump -d`.
6. Note the false-positive trap: GAS mnemonics are case-insensitive, so
   `MOVL` is fine in GAS but a NASM label case change is not.

## What to verify

- File assembles with the target toolchain; good forms exit 0, bad forms exit
  nonzero with the documented error.
- `objdump -d` encodings match intent for both dialects of the same code
  (identical bytes, reversed text order).
- No `mov rax, buf` where `[buf]` (or `lea`) is meant; no `inc [counter]`
  without a size hint; `default rel` present for Mach-O/PIE.
- NASM examples: bad ones fail exactly as documented (researched — run
  `nasm -f elf64` on a host with NASM).

## How to verify

```
# Source-backed (this host):
gcc -O2 -S good/src.c                 # AT&T output
gcc -O2 -masm=intel -S good/src.c     # Intel-syntax output
gcc -c good/att_correct.s             # exit 0
gcc -c good/intel_syntax.s            # exit 0 (GAS .intel_syntax noprefix)
gcc -c bad/att_order.s                # exit 1: operand type mismatch
objdump -d good/att_correct.o         # b8 05 00 00 00 vs 8b 04 25 05 00 00 00

# Researched (NASM absent here) — run on a NASM host:
nasm -f elf64 bad/nasm_case.asm       # exit 1: symbol not defined
nasm -f elf64 bad/nasm_size.asm       # exit 1: operation size not specified
nasm -f elf64 good/nasm_*.asm         # exit 0
```

## Where the knowledge comes from

- `nasm-manual` §3.1 (syntax, case sensitivity), §3.3 (addressing), §4
  (relocations, `default rel`), directives.
- `binutils-docs` — GNU `as` AT&T and `.intel_syntax noprefix` dialects.
- `sysv-amd64-abi` §3.2, §4.4 (register roles, RIP-relative relocations).
- `intel-sdm` Vol.2 (instructions, operand encodings).
- Failure survey `research/2026-08-15-asm-agent-failures-survey.md` — ocrosby
  PR#33 (4 NASM error classes), BBoeOS PR#506 (`$` in directives), AT&T
  inversion (ASM-1).

## Related skills

- `asm-verification-hallucination-gate` — verifies dialect-correct code
  actually encodes right.
- `asm-x86-64-registers-and-addressing` — register widths and addressing modes.
- `asm-inline-asm-constraints` — which dialect GCC expects inside C.
- `asm-calling-conventions` — argument/stack layout.

## Evaluation

- Synthetic: bad NASM files (case, brackets, size hint, `default rel`, `$`)
  must be flagged; bad GAS files (inverted order, missing `$`) must be caught
  by the assembler or review.
- False-positive: good GAS files (AT&T correct, Intel-syntax mode, uppercase
  mnemonic) and good NASM files must NOT be flagged.
- Historical: ocrosby PR#33 (four classes), BBoeOS PR#506 (`$` in directives)
  reproduced as `examples/bad/`.
- Adversarial: `bad/att_immediate.s` assembles cleanly (exit 0) but loads from
  address 5 instead of constant 5 — silent wrong semantics.
- Commands and recorded output: `evals/README.md`.
