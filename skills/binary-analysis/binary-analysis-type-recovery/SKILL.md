---
name: binary-analysis-type-recovery
description: Use when recovering C types (char, short, int, long, float, double, structs, arrays, pointers, function/vtable signatures) from x86-64 disassembly via instruction-width and addressing patterns (movzx/movsx, movss/movsd, movl/movq, disp(%reg), scale indexing), validated with DWARF when present.
---

# Binary Analysis: Type Recovery from Assembly

## When to use

- Reverse-engineering stripped or optimized binaries and reconstructing function
  signatures and struct layouts.
- Reading `objdump -d` / Ghidra / IDA output and deciding whether a value is a
  `char`, `int`, `long`, pointer, float, or double.
- Rebuilding a struct from its access pattern (`disp(%reg)` offsets) or an array
  from its stride/indexed addressing.
- Distinguishing direct calls, indirect calls through function-pointer params,
  and vtable/virtual dispatch.
- Cross-checking type guesses with DWARF in `-g` binaries.

## When not to use

- Reading a specific compiler's optimizer tricks (inlining, tail calls, DCE,
  lea strength reduction) — use `asm-optimizer-artifacts`.
- Registers/addressing-mode mechanics, REX, suffixes, flags — use
  `asm-x86-64-registers-and-addressing`.
- Recovering source VARIABLE names or lines from debug info — use
  `dwarf-debug-info`.
- AArch64/RISC-V/32-bit x86 — instruction set and calling conventions differ.
- C++ class layout/mangling detail — use `abi-layout-reasoning` + itanium-cxx-abi.

## What the agent often gets wrong

- "movsbl copies a byte, so the type is char." The result is a 32-bit int; the
  `s`/`z` tells you the SOURCE was signed/unsigned. (See rule 1.)
- "movsd loads a string." In FP context `movsd` is a scalar double load.
- "movl means 64-bit long." AT&T `l` = 32-bit, `q` = 64-bit. And `long` is 32-bit
  on Windows (LLP64), 64-bit on Linux (LP64).
- "8-byte load = pointer." It is a u64/long long unless the value is dereferenced
  or called.
- "Struct field at offset 8 is the second member." Padding and alignment make
  offsets non-contiguous; recover the map, not a guess.
- "`jmp *%rax` is not a call." It is an indirect tail call; a `mov disp(%reg),%reg`
  just before it is a vtable/function-pointer dispatch.
- "First argument is always in rdi." SysV only; Windows x64 uses rcx, rdx, r8, r9.
- Guessing types without checking DWARF when it exists.

## How to reason correctly

1. Identify the ABI first (ELF -> SysV: `rdi,rsi,rdx,rcx,r8,r9`; PE/COFF ->
   Windows x64: `rcx,rdx,r8,r9`) and the target data model (LP64 vs LLP64).
2. For each register value, collect every use: width (l/q/ss/sd), whether it is
   dereferenced (`disp(%reg)`, `(base,idx,scale)`), whether it is called
   (`call *reg` / `jmp *reg`), and what arithmetic is applied.
3. Classify narrow extensions: `movs*` = signed source, `movz*` = unsigned
   source; suffix width = source width; result is int/long.
4. Classify FP: `movss`/`addss` = float, `movsd`/`mulsd` = double; args in xmm.
5. Classify structs: assemble the `offset -> width -> type` map from all memory
   accesses to one base; fill gaps with alignment rules; confirm via DWARF.
6. Classify arrays: scale (1/2/4/8) is the element size; signedness of the index
   comes from `movslq` vs plain `mov`.
7. Classify calls: load-then-jump = function pointer or vtable; find the slot
   offset to identify which method.
8. When `-g` debug info exists, treat DWARF DIEs as the ground truth and compare
   every recovered type (see references/type-recovery.md rule 9).

## What to verify

- Every recovered narrow integer: the extension mnemonic (`movsbl` vs `movzbl`)
  and the runtime value for 0xFF/0xFFFF.
- Every recovered FP type: the instruction suffix (`ss` vs `sd`).
- Struct layouts: every field offset and the byte_size (padding included).
- Array element size: the scale or the loop stride.
- Function signatures: which registers are read-before-write, and the return
  register.
- Indirect calls: whether the target came from memory (vtable) or a parameter.
- Cross-check with DWARF when present; mark results verified vs inferred.

## How to verify

```
gcc -O2 -g -c type_recovery.c -o t.o
objdump -d t.o                 # instruction patterns
objdump --dwarf=info t.o       # DIE types, member offsets, byte_size
gcc -O2 -x c -o run.exe - <<'EOF'   # runtime check of extension semantics
int f(char c){return c;}
EOF
```

Feed `0xFF`/`0xFFFF` to a small test main: `movsbl`-recovered code must yield
`-1`/`-1`, `movzbl`-recovered must yield `255`/`65535` (recorded). Full recorded
patterns and commands: `references/type-recovery.md` and `evals/README.md`.

## Where the knowledge comes from

- `intel-sdm` — MOVSX/MOVZX, MOV, MOVSS/MOVSD, LEA, CALL/JMP (Vol.2); addressing
  modes and zero-extension rule (Vol.1 §3.4, §3.7.5)
- `sysv-amd64-abi` — §3.2 calling sequence, §3.2.3 FP args, §4.1 data sizes (LP64)
- `dwarf-v5` — DW_AT_data_member_location, DW_AT_byte_size, subprogram DIEs
- `binutils-docs` — objdump `-d`, `--dwarf=info`
- `gcc-manual` — `-g`, `-O2`
- Empirical: GCC 16.1 (MinGW x86-64), recorded 2026-08-14

## Related skills

- `asm-x86-64-registers-and-addressing` — width suffixes, movzx/movsx, addressing
  (require of)
- `asm-calling-conventions` — which register is which argument per ABI
- `asm-optimizer-artifacts` — lea is arithmetic, `jmp` tail calls, -O2 shapes
- `dwarf-debug-info` — reading DIEs to verify recovered types (recommend)
- `abi-layout-reasoning` — struct padding/alignment rules behind offsets
- `elf-linker-loader-debugger` — symbol/relocation context around the disassembly

## Evaluation

Synthetic: recover signatures/layouts from the recorded disassembly in
`examples/good/type_recovery.dis` — every type must match the C source in
`type_recovery.c` (movsbl->char, movzbl->uchar, movswl->short, addss->float,
mulsd->double, Big offsets 0/4/6/8, scale-4 int array, Windows-x64 arg order,
vtable slot at 8).
False-positive: `movl` must NOT be reported as 64-bit; `lea` result must NOT be a
pointer; an 8-byte undereferenced value must NOT be a pointer; `movswl` on an
LLP64 target must NOT be called a bug.
Adversarial: the misreadings in `examples/bad/type_recovery.c` (struct offset,
pointer-as-int, ignored movsx sign) must each be caught and corrected.
Runtime: 0xFF/0xFFFF edge values must match the movs/movz distinction.
Commands and verified facts: `evals/README.md`.
