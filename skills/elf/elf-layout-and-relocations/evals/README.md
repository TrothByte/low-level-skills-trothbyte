# Evaluation — elf-layout-and-relocations

Skill: `skills/elf/elf-layout-and-relocations`. Stability target: `evaluated`.

## Synthetic evals

- **easy/positive**: read a `readelf -h` excerpt and state class, endianness, `e_type`,
  `e_machine`, `e_entry` from `e_ident` — must not assume native endianness.
- **easy/negative**: `.bss` must be recognized as `SHT_NOBITS` (file size 0, memory size
  non-zero); a claim that `.bss` occupies file space must be corrected.
- **medium/positive**: given symbols from `readelf -s`, classify binding
  (LOCAL/GLOBAL/WEAK) and visibility (default/hidden/protected), and predict which ones
  appear in `.dynsym` of a `.so`.
- **medium/negative**: compute `R_X86_64_PC32 = S + A - P` for a concrete `call` and
  verify the displacement fits 32 bits; given `readelf -r` output with
  `R_X86_64_64`/`PLT32`/`GOTPCREL`, name each formula.
- **hard/negative**: explain the `relocation R_X86_64_32S ... recompile with -fPIC`
  error — fix is the compile flag, not a linker flag; and state that on MinGW/PE the same
  source needs no `-fPIC` (VERIFIED).
- **adversarial**: `nm` letters only (`T`/`D`/`B`/`b`/`r`/`U`) on a stripped binary —
  predict the export set and which symbol is a local static vs a global.

## False-positive evals (correct code must NOT be flagged)

- A stripped executable showing only `.dynsym` is normal, not "missing symbols".
- PC-relative-only PIC code (`R_X86_64_PC32`/`PLT32`) in a `.so` is correct; do NOT flag
  it as text relocation.
- `static` data and functions (local symbols) are correct; do NOT flag them as "unused".
- `examples/good` (sections.c, libgeom) must build clean under `-Wall -Wextra -Werror -O2`
  and run with exit 0.
- MinGW/PE builds without `-fPIC` are correct; do NOT demand `-fPIC` on PE/COFF.

## Verification commands

On this host (MinGW, PE/COFF):

```
gcc -Wall -Wextra -Werror -O2 -c examples/good/sections.c -o sections.o   # exit 0
gcc -Wall -Wextra -Werror -O2 -c examples/good/libgeom.c -o libgeom.o     # exit 0
gcc -Wall -Wextra -Werror -O2 -c examples/good/main.c -o main.o           # exit 0
gcc main.o libgeom.o sections.o -o good.exe && ./good.exe                 # exit 0
objdump -h sections.o         # .text/.data/.bss/.rdata present
nm sections.o         # T compute / T main / D initialized / B uninitialized
                      # R hello / R table / U printf (t hidden_helper, d secret at -O0)
objdump -r sections.o         # IMAGE_REL_AMD64_REL32 printf
gcc examples/bad/symbol-conflict/a.c examples/bad/symbol-conflict/b.c -o b1.exe  # exit 1
gcc examples/bad/undefined-symbol/main.c -o b2.exe                               # exit 1
gcc -fno-pic -shared -o libnopic.dll examples/bad/missing-fpic/libnopic.c        # exit 0
```

On an ELF host (Linux or WSL; requires readelf):

```
gcc -Wall -Wextra -Werror -O2 -c examples/good/sections.c -o sections.o
readelf -h sections.o        # e_ident magic, class, endianness, e_type=ET_REL
readelf -S sections.o        # .bss SHT_NOBITS, .rodata SHF_ALLOC only
readelf -s sections.o        # binding/visibility
readelf -r sections.o        # R_X86_64_PC32/PLT32
gcc -fno-pic -shared -o libnopic.so examples/bad/missing-fpic/libnopic.c
#    relocation R_X86_64_32S ... recompile with -fPIC  -> exit 1
gcc -fPIC -shared -o libnopic.so examples/bad/missing-fpic/libnopic.c  # exit 0
```

## Verified facts (this repository's host: MinGW gcc 16.1, binutils 2.46, PE/COFF)

| Fact | Result | How verified |
|---|---|---|
| `sections.o` builds clean with `-Wall -Wextra -Werror -O2` | exit 0 | gcc |
| Static link `main.o libgeom.o sections.o` + run | exit 0; `area = 12, global = 100`; `hello, elf world 82 8` | gcc + run |
| `.text`/`.data`/`.bss`/`.rdata` sections in object | present | objdump -h |
| `compute`/`main` global functions, `hidden_helper` local | `T compute`, `T main`, `t hidden_helper` (at -O0; folded away at -O2) | nm |
| `initialized` global data, `secret` static data | `D initialized`, `d secret` (at -O0) | nm |
| `uninitialized` BSS symbol | `B uninitialized` | nm |
| `hello`/`table` read-only symbols | `R hello`, `R table` (global read-only) | nm |
| `printf` undefined reference | `U printf` | nm |
| Object-file call placeholder + relocation | `call ...` + `IMAGE_REL_AMD64_REL32 printf` | objdump -d/-r |
| `multiple definition of 'helper'` | exit 1, ld names both files | gcc link |
| `undefined reference to 'never_defined_here'` | exit 1, ld | gcc link |
| `-fno-pic -shared` on MinGW | exit 0, runs, exit 0 (no PIC needed on PE) | gcc + run |

## Documented-as-target facts (ELF host required, NOT executed on this host)

| Claim | Expected result |
|---|---|
| `e_ident` magic `0x7f 'E' 'L' 'F'`, EI_CLASS, EI_DATA | visible in `readelf -h` |
| `ET_REL` for `.o`, `ET_DYN` for PIE/`.so`, `ET_EXEC` for static exec | `readelf -h` `e_type` |
| `EM_X86_64 = 62` | `readelf -h` `e_machine` |
| `e_entry` is `_start`, not `main` | `readelf -h` vs `nm` |
| `.bss` is `SHT_NOBITS` | `readelf -S` |
| `.rodata` not writable (`SHF_ALLOC` only, no `PF_W`) | `readelf -S`/`-l` |
| `.symtab` removed by `strip`, `.dynsym` survives | `readelf -s` before/after strip |
| `R_X86_64_64 = S+A`, `R_X86_64_PC32 = S+A-P`, `R_X86_64_PLT32 = L+A-P`, `R_X86_64_GOTPCREL = G+GOT+A-P` | `readelf -r` + computation |
| `-fno-pic -shared` with `&global` → `relocation R_X86_64_32S ... recompile with -fPIC` | gcc link exit 1 |
| `-fPIC` replaces 32S with GOTPCREL/PC-relative | `readelf -r` on the `.so` |
| Dynamic binary has `PT_INTERP` + `DT_NEEDED`; static does not | `readelf -l`/`-d` |

## Scoring (for routing eval)

- precision: every `readelf`/`nm`/relocation claim must match the cited rule and formula.
- recall: each bad case (32S, multiple definition, undefined symbol) must be detected.
- FP-rate: `examples/good` and the false-positive list produce zero findings.
- platform correctness: ELF-only claims (readelf, R_X86_64_*, PIC error) are stated as
  requiring an ELF host; PE/COFF-verified facts are labeled VERIFIED.
