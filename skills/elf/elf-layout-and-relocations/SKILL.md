---
name: elf-layout-and-relocations
description: Use when reading or debugging ELF object files and executables — ELF header fields, section vs program headers, .text/.data/.bss/.rodata/.dynsym/.got/.plt roles, symbol binding and visibility, R_X86_64_* relocation types, static vs dynamic linking, and the "recompile with -fPIC" error on x86-64 shared objects.
---

# ELF Layout & Relocations

## When to use

- Explaining `readelf -h/-S/-l/-s/-r` output on an ELF object, executable, or `.so`.
- Reading a symbol table: `.symtab` vs `.dynsym`, `STB_LOCAL`/`STB_GLOBAL`/`STB_WEAK`, visibility.
- Reading a relocation table: `R_X86_64_64`, `R_X86_64_PC32`, `R_X86_64_PLT32`, `R_X86_64_GOTPCREL`.
- Deciding what the compiler actually wrote into which section (`-S` asm, section directives).
- Diagnosing why an x86-64 shared-object link fails with `relocation R_X86_64_32S ... recompile with -fPIC`.
- Telling apart static vs dynamic linking consequences (which sections exist, which symbol table matters).
- Building the prerequisite knowledge for `elf-linker-loader-debugger`, `elf-dynamic-linking-got-plt`, `embedded-linker-script`.

## When not to use

- Windows PE/COFF internals (import libs, `.pdata`, base relocations) — cross-check host only.
- Linker options, dynamic loader search paths, lazy-binding failure timing — use `elf-linker-loader-debugger`.
- DWARF debug info — use `dwarf-debug-info`.
- Function calling conventions, struct layout — use `abi-layout-reasoning`.
- AArch64/RISC-V relocation numbers — this skill is x86-64-specific for relocation types.

## What the agent often gets wrong

- "The ELF header is the same as the section list." The header describes class/endianness/type and points to three tables; the section list is one of them, and program headers are a different view for the loader.
- "Endianness is always little-endian." `e_ident[EI_DATA]` decides; `readelf` reads it from the file, an agent must not assume.
- "`.bss` takes file space." `.bss` has `NOBITS` (sh_type `SHT_NOBITS`): file size 0, memory size non-zero.
- "`.rodata` is writable." It is typically in a read-only `PT_LOAD` segment; writing there segfaults.
- "Every symbol is in `.symtab`." `strip` removes `.symtab`; the dynamic loader only sees `.dynsym`. A `readelf -s` on a stripped binary shows `.dynsym` only.
- "Relocations are addresses." A relocation is `(symbol, type, addend)`; the type defines the formula that turns symbol value into bytes.
- "`-fPIC` is a performance flag." On x86-64 ELF it is a correctness requirement for shared objects; without it the linker rejects absolute relocations.
- "`main` is `e_entry`." `e_entry` is `_start` for a dynamically linked executable; `main` is reached through startup code.

## How to reason correctly

1. Identify the ELF class first (`ELFCLASS32`/`ELFCLASS64`) and endianness from `e_ident`; all offsets and values are read per that class and endianness.
2. Separate the three views: ELF header → section headers (`readelf -S`, for the linker/tools) vs program headers (`readelf -l`, for the loader) vs symbol/relocation tables.
3. Classify every symbol: name, `st_info` binding (`STB_LOCAL=0`/`GLOBAL=1`/`WEAK=2`) and type, `st_visibility` (default/hidden/protected), section index. Then answer: which table is it in (`.symtab`/`.dynsym`), and who consumes it.
4. For every relocation, write the formula from the x86-64 psABI, plug in `S` (symbol value), `A` (addend), `P` (place), and check the result range fits the field width.
5. Decide link model: static (all `.symtab` resolved at link, no `DT_NEEDED`) vs dynamic (`.dynsym` + `DT_NEEDED`, loader resolves at load).
6. For shared objects, ask "is this piece of code position-independent?" — any absolute relocation (`R_X86_64_64`/`R_X86_64_32S`) in a `.so` is a bug or a PIC failure.

## What to verify

- `readelf -h`: magic `\x7fELF`, class, endianness, `e_type` (REL/EXEC/DYN), `e_machine` (EM_X86_64 = 62), `e_entry`.
- `readelf -S`: `NOBITS` on `.bss`, `SHF_ALLOC` on runtime sections, absence of `.symtab` after `strip`.
- `readelf -s`: `.dynsym` present with only exported/default-visibility globals in a `.so`.
- `readelf -r`: relocation types match the access pattern (absolute vs PC-relative vs GOT).
- Link success/failure of the good and bad examples; exact linker message for the `-fPIC` case.
- On non-ELF hosts: confirm which claims were verified on PE/COFF (section roles, symbol kinds, static/dynamic link) vs documented-as-target for ELF.

## How to verify

On an ELF host (Linux/WSL), requires `readelf`:

```
gcc -c good.c -o good.o
readelf -h good.o        # e_ident, e_type=ET_REL, e_machine=EM_X86_64
readelf -S good.o        # .text/.data/.bss(NOBITS)/.rodata/.rela.text
readelf -s good.o        # binding/visibility per symbol
readelf -r good.o        # relocation types on a call and on &global
gcc -fPIC -shared -o libx.so bad_nopic.c   # expect R_X86_64_32S ... recompile with -fPIC
gcc -shared -o libx.so good_pic.c          # exit 0
```

On this repository's host (MinGW gcc 16.1, PE/COFF), what CAN be checked:

```
gcc -Wall -Wextra -Werror -O2 -c sections.c -o sections.o
nm sections.o            # T/D/b/U per symbol kind (PE mirrors ELF semantics)
objdump -h sections.o    # .text/.data/.bss/.rdata present
objdump -r main.o        # IMAGE_REL_AMD64_REL32 placeholder, like a PC-relative ELF reloc
gcc a.c b.c -o bad.exe   # multiple definition  -> exit 1 (VERIFIED)
gcc main.c undefined.c   # undefined reference   -> exit 1 (VERIFIED)
```

ELF-specific facts (e_ident layout, `R_X86_64_*` numbers, `readelf` output) are recorded as documented-as-target; do NOT claim `readelf` was run on ELF on this host.

## Where the knowledge comes from

- System V ABI — ELF: ELF header, sections/segments, symbol table, relocation section, dynamic section.
- System V AMD64 psABI: relocation types (`R_X86_64_*` formulas), position-independent code, shared-object rules.
- GNU binutils documentation: readelf/objdump/nm output semantics.
- GNU ld manual: linking shared objects, `-fPIC` error context.

## Related skills

- `elf-linker-loader-debugger` — consumes this skill (require of).
- `elf-dynamic-linking-got-plt` — extends GOT/PLT mechanics and lazy binding.
- `embedded-linker-script` — consumes this skill via linker scripts for bare metal.
- `abi-layout-reasoning` — adjacent ABI context (recommend to it).
- `dwarf-debug-info` — the debug side of the same object file.

## Evaluation

Synthetic: read `readelf` excerpts and classify header fields; map symbol binding/visibility; compute relocation results with the `S + A - P` formulas; detect `.bss` NOBITS and stripped `.symtab`; classify each bad example's link failure. Adversarial: given only `nm` letters or a partial `readelf -s`, predict which symbols a `.so` exports. False-positive: correct `-fPIC` shared-object builds, stripped binaries with only `.dynsym`, and PC-relative-only code must NOT be flagged. See `evals/README.md`.
