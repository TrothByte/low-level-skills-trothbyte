---
name: asm-x86-64-registers-and-addressing
description: Use when reading, writing, or reviewing x86-64 assembly: choosing register widths, operand-size suffixes, addressing modes, RIP-relative operands, REX encoding, canonical 48-bit addresses, or zero/sign extension. Prevents wrong-size moves, stale-flag branches, and non-canonical-address bugs in hand-written asm and inline asm.
---

# x86-64 Registers and Addressing

## When to use

- Reading, writing, or debugging x86-64 assembly (`.s` files, disassembly, inline asm).
- Diagnosing a wrong-value or fault bug that reduces to width, flag, or addressing.
- Checking RIP-relative/PIC code, `r8`-`r15`, `movzx`/`movsx`, or hand-encoded bytes.

## When not to use

- AArch64/RISC-V/32-bit x86 — different registers, flags, and addressing rules.
- Call argument/stack layout — use `asm-calling-conventions`; constraint syntax —
  use `asm-inline-asm-constraints`.
- Microarchitectural latency/throughput tuning — use `agner-fog` source tables.

## What the agent often gets wrong

- `movl` read as "long=64-bit"; in GAS AT&T `l` is 32-bit, `q` is 64-bit.
- A 32-bit write leaves garbage above — in 64-bit mode it zero-extends.
- `lea` sets flags — it does not; a branch after `lea` reads stale flags.
- `jb` and `jl` are interchangeable — `jb` is unsigned (CF), `jl` is signed (SF vs OF).
- RIP-relative disp32 is from the instruction start — it is from the next RIP.
- Non-canonical addresses like 0x0000800000000000 work — they fault with #GP.
- `movsbl` sign-extends to 64 — it reaches only 32 bits, then zero-extends.
- `rsp` can be an index — SIB index 100b means "no index".
- `movq $0xFFFFFFFF,%rax` is compact — the C7 imm32 form sign-extends; GAS emits
  a 10-byte `movabs` instead.

## How to reason correctly

1. Match suffix to register width (`.b`/`.w`/`.l`/`.q`); mismatch is an error,
   silent truncation is a runtime bug.
2. Choose the extension by destination width: `movslq`/`movsbq` for signed,
   `movl`/`movz*` for zero.
3. Name the flags per branch: unsigned CF/ZF → `ja`/`jb`/`jae`/`jbe`; signed SF/OF
   → `jg`/`jl`/`jge`/`jle`; `jz` → ZF.
4. Classify each memory operand: base+disp, base+idx*scale+disp (scale 1/2/4/8),
   or RIP-relative; one base, one index.
5. Check canonicality of hand-built addresses (bit 47 sign rule); confirm encodings
   with `objdump -d`, never by reading the mnemonic.

## What to verify

- File assembles; `objdump -d` shows the intended encodings (REX, size, relocation).
- No conditional branch right after `lea`/`mov`/`push` (no flag update).
- Signed data uses signed branches, unsigned data unsigned branches.
- Hand-formed addresses stay in the canonical user range.
- Inline-asm C compiles with `-Wall -Wextra -Werror -O2 -c`.

## How to verify

```
as file.s -o file.o           # or: gcc -c file.s
objdump -dr file.o            # encodings and relocations
gcc -Wall -Wextra -Werror -O2 -c inline_asm.c
```

For runtime behavior: single-step in gdb (`p/x $rax`, `$rflags`) and feed edge
values (-1, 0xFFFFFFFF, 0xFF) to the extension snippets.

## Where the knowledge comes from

- `intel-sdm` Vol.1 §3.4 (registers/rflags), §3.6 (flags), §3.7.5-3.7.6 (addressing,
  RIP-relative); Vol.2 (REX, MOV, MOVSX/MOVZX, C7 /0); Vol.3A §3.3.7.1 (canonical).
- `amd64-apm` Vol.1 §3, Vol.2 (REX, operand size), Vol.3 (MOVSX/MOVZX), §5.3 (canonical).
- `sysv-amd64-abi` §3.2 (register roles), §4.4 (relocations).
- `agner-fog` instruction tables; `binutils-docs` GNU `as` AT&T suffixes.

## Related skills

- `asm-calling-conventions` — register roles (recommends this skill)
- `asm-inline-asm-constraints`, `asm-signed-unsigned-branches` — require this skill
- `asm-optimizer-artifacts` — reading compiler-generated asm
- `simd-vectorization-cross-layer` — requires this skill

## Evaluation

- Synthetic: bad `.s` cases (size mismatch, truncation, flag misuse, scale error,
  rsp-index, sign-extension, hand-encoding) must be caught; good cases must assemble
  with the intended encodings.
- False-positive: correct `cmp`+`jl`, deliberate `movl` 32-bit load, valid
  base+index*scale and RIP-relative forms must NOT be flagged.
- Adversarial: `bad/canonical.s` assembles cleanly but faults at runtime — recognize
  it as #GP, not an assembly error.
- Commands and recorded results: `evals/README.md`.
