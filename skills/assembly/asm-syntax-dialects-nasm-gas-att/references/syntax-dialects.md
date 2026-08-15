# NASM vs GAS/AT&T vs GAS/Intel — Syntax Dialects Reference

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to
registry/sources.yaml.

## 1. Three dialects, one instruction set

- **RULE**: x86-64 assembly has two families of textual syntax. NASM/MASM
  write Intel-style: `mov eax, [rdi]`, `imul eax, ecx, 38`. GNU `as` writes
  AT&T by default: `movl (%rdi), %eax`, `imull $38, %ecx, %eax`, and can also
  switch to an Intel-ish dialect with `.intel_syntax noprefix` (no `%` on
  registers). The machine encodings are identical — only the text differs.
- **WHY AI GETS IT WRONG**: models trained mostly on one dialect emit the
  other's operand order or mix the two (a recorded failure emitted
  `movl 0x0, -0x4(%rbp)` in GAS AT&T meaning "store 0" — reversed operands and
  missing `$`).
- **CORRECT REASONING**: pick the dialect of the target toolchain, then apply
  its own rules; never combine AT&T order with Intel memory syntax.
- **EXAMPLE** (bad): `movl 0x0, -0x4(%rbp)` → `as: operand type mismatch`.
- **COUNTEREXAMPLE** (good): `movl $0, -0x4(%rbp)` (AT&T) ≡ `mov DWORD PTR
  [rbp-4], 0` (GAS Intel) ≡ `mov dword [rbp-4], 0` (NASM).
- **VERIFICATION**: `gcc -S` (AT&T) vs `gcc -S -masm=intel`; `gcc -c` + objdump
  shows identical encodings for the good forms.
- **SOURCE**: binutils-docs (GNU as dialects); nasm-manual §3.1; sysv-amd64-abi.

## 2. AT&T operand order and `$` for immediates

- **RULE**: in AT&T, the source operand comes first, destination last:
  `movl %ebx, %eax` moves ebx INTO eax. Immediates are `$value`; a bare number
  is an absolute memory address. `movl 5, %eax` loads from address 5; `movl $5,
  %eax` loads constant 5.
- **WHY AI GETS IT WRONG**: Intel-trained models write `mov eax, ebx`-style
  order (dest first) and forget `$`, producing reversed semantics or
  memory-to-memory errors.
- **CORRECT REASONING**: name source and destination per line. Check that the
  intended flow (value → target) matches `src, dst`.
- **EXAMPLE** (bad): `movl 5, %eax` → objdump `8b 04 25 05 00 00 00` =
  `mov 0x5,%eax` (loads from address 5, silently).
- **COUNTEREXAMPLE** (good): `movl $5, %eax` → `b8 05 00 00 00`.
- **VERIFICATION**: objdump -d distinguishes `8b 04 25 05 00 00 00` from
  `b8 05 00 00 00` (both recorded in evals).
- **SOURCE**: binutils-docs; intel-sdm Vol.2 (MOV); sysv-amd64-abi.

## 3. NASM label case-sensitivity

- **RULE**: NASM labels are case-sensitive: `Loop`, `loop`, and `LOOP` are
  three different identifiers. Mnemonics are case-insensitive (NASM manual:
  "all labels are case-sensitive"). GAS, by contrast, accepts both cases for
  mnemonics too.
- **WHY AI GETS IT WRONG**: four documented NASM error classes include a loop
  whose `jnz` target switched case — the branch silently targets a different
  label or a nonexistent one.
- **CORRECT REASONING**: keep every reference to a label byte-identical to its
  definition; treat a case mismatch as a different symbol, not a cosmetic
  difference.
- **EXAMPLE** (bad): `jnz loop` when the label is defined as `.loop`.
- **COUNTEREXAMPLE** (good): `jnz .loop` matching the `.loop:` definition.
- **VERIFICATION**: `nasm -f elf64` (researched — nasm not installed here;
  expected: "error: symbol `loop' not defined" or a stray jump).
- **SOURCE**: nasm-manual §3.1 (case sensitivity).

## 4. NASM address vs content: `buf` vs `[buf]`

- **RULE**: in NASM, `mov rax, buf` loads the ADDRESS of `buf` (a lea
  equivalent); `mov rax, [buf]` loads the VALUE stored at `buf`. Dropping the
  brackets silently changes the semantics — "not an error, just not what you
  meant" (ocrosby PR#33 class 2).
- **WHY AI GETS IT WRONG**: mixed conventions — some dialects spell "address"
  with brackets; models transfer that and produce address-where-value bugs.
- **CORRECT REASONING**: brackets = memory dereference; no brackets = effective
  address (mov) — identical to `lea`.
- **EXAMPLE** (bad): `mov rax, buf` when the value 42 in `buf` is wanted.
- **COUNTEREXAMPLE** (good): `mov rax, [buf]`; or `lea rax, [buf]` when the
  address is intended.
- **VERIFICATION**: `nasm -f elf64` + objdump shows `lea`-style absolute
  addressing for the bracketless form (researched).
- **SOURCE**: nasm-manual §3.1, §3.3.

## 5. NASM size hints

- **RULE**: NASM requires an explicit size hint whenever the operand size is
  not inferable from a register: `inc qword [counter]`, `mov dword [x], eax`
  is fine (eax gives the size) but `mov dword [x], 1` needs `dword`. Omitting
  it produces "operation size not specified".
- **WHY AI GETS IT WRONG**: GAS infers sizes from register widths and suffixes,
  so models trained on GAS forget NASM's hint requirement.
- **CORRECT REASONING**: for any memory operand without a register
  co-operand, spell byte/word/dword/qword.
- **EXAMPLE** (bad): `inc [counter]` → NASM: "error: operation size not
  specified".
- **COUNTEREXAMPLE** (good): `inc qword [counter]`.
- **VERIFICATION**: `nasm -f elf64` expected error vs clean (researched).
- **SOURCE**: nasm-manual §3.1; nasm-manual error messages.

## 6. NASM `default rel` (Mach-O / PIE)

- **RULE**: without `default rel`, NASM uses absolute addressing for symbol
  references. Mach-O (macOS) binaries cannot hold absolute relocations in
  position-independent code, and PIE linking may reject or mis-resolve them.
  `default rel` near the top selects RIP-relative addressing.
- **WHY AI GETS IT WRONG**: the directive exists only in NASM; models miss it
  because GAS/AT&T is RIP-relative by default.
- **CORRECT REASONING**: on Mach-O/ELF PIE, prefer RIP-relative; make the
  default explicit at file top.
- **EXAMPLE** (bad): `mov rax, msg` with no `default rel`.
- **COUNTEREXAMPLE** (good): `default rel` before `mov rax, msg`.
- **VERIFICATION**: `nasm -f macho64`/`-f elf64` (researched — expected
  relocation-type errors on Mach-O for the bad form).
- **SOURCE**: nasm-manual §4 (relocations); sysv-amd64-abi §4.4 (RIP-relative).

## 7. NASM 3.x: `$` is not valid in directive arguments

- **RULE**: `global $main` / `extern $puts` were accepted in NASM 2.x but the
  `$` is invalid in NASM 3.x directive arguments (BBoeOS PR#506). Inside
  expressions `$` remains the current-location counter.
- **WHY AI GETS IT WRONG**: stale NASM-2.x memory; `$` looks like a harmless
  prefix to a symbol.
- **CORRECT REASONING**: directive arguments take bare symbol names; `$`
  appears only in expressions.
- **EXAMPLE** (bad): `global $main`.
- **COUNTEREXAMPLE** (good): `global main`.
- **VERIFICATION**: `nasm -f elf64` (researched — expected 3.x error).
- **SOURCE**: nasm-manual (directives); BBoeOS PR#506 (survey ASM-9).

## Quick reference table

| Topic | NASM | GAS AT&T | GAS Intel |
|---|---|---|---|
| Operand order | dst, src | src, dst | dst, src |
| Immediate | `mov eax, 5` | `movl $5, %eax` | `mov eax, 5` |
| Memory | `mov eax, [rdi]` | `movl (%rdi), %eax` | `mov eax, [rdi]` |
| Labels | case-sensitive | case-insensitive | case-insensitive |
| Size hints | required for mem-only | via suffix | `BYTE/WORD/DWORD/QWORD PTR` |
| RIP-relative | needs `default rel` | default for symbols | default |
| `$` in directives | invalid in 3.x | n/a | n/a |
