# examples/good — correctly compiled and linked ELF-building sources

These sources compile cleanly (`-Wall -Wextra -Werror -O2`) and produce the expected
sections, symbol kinds, and a correct static link. They exercise the rules in
`references/elf-layout.md`.

## Files

- `sections.c` — one translation unit showing every common section: `.text` (`compute`,
  `main`), `.rodata` (`hello`, `table`), `.data` (`initialized`, static `secret`),
  `.bss` (`uninitialized`, no file space), plus a global (`compute`), a static local
  (`hidden_helper`, `secret`), and an undefined reference to `printf` (`U`).
- `libgeom.c` + `libgeom.h` — a library with one exported global function (`geom_area`)
  and one file-local static (`calls`).
- `main.c` — the executable; defines `global_from_main` (a `.data` global) and calls
  into the library.

## Verified build (MinGW gcc 16.1 / binutils 2.46, PE/COFF)

Section view — `objdump -h sections.o` shows `.text`, `.data`, `.bss`, `.rdata`
(rodata-equivalent) (VERIFIED):

```
objdump -h sections.o
# Sections: Idx Name          Size      VMA               File off  Algn
#   0 .text ...
#   1 .data ...
#   2 .bss  ...  <no file content — PE .bss is zero-fill, like ELF SHT_NOBITS>
#   3 .rdata ...
```

Symbol view — `nm sections.o` shows the binding/visibility ground truth (VERIFIED at `-O0`,
where the optimizer has not folded away the static symbols):

```
gcc -O0 -c sections.c -o sections_o0.o && nm sections_o0.o
# 0000000000000000 T compute          <- global function (STB_GLOBAL analog)
# 000000000000001d T main             <- global entry function
# 000000000000000e t hidden_helper    <- STB_LOCAL analog (static function)
# 0000000000000000 D initialized      <- global data
# 0000000000000004 d secret           <- local data (static)
# 0000000000000000 B uninitialized    <- BSS symbol
# 0000000000000020 R hello            <- global read-only data
# 0000000000000000 R table
#                  U printf           <- undefined, resolved by the linker
```

At `-O2` the compiler folds `hidden_helper(secret)` and `secret` away (both disappear
from `nm`), which is itself a teaching point: symbols that are `static` and fully
constant-folded never reach the symbol table. `hello`/`table` remain visible (`R`).

Relocation view — the call to `printf` is a placeholder plus a relocation record
(VERIFIED):

```
objdump -r sections.o
# [.text.startup] OFFSET ... TYPE ... VALUE
#   ... IMAGE_REL_AMD64_REL32  printf    <- PC-relative, the PE analog of R_X86_64_PC32
#   ... IMAGE_REL_AMD64_REL32  .data     <- access to initialized
#   ... IMAGE_REL_AMD64_REL32  .bss      <- access to uninitialized
#   ... IMAGE_REL_AMD64_REL32  .rdata    <- access to hello / table
```

Static link of two objects (VERIFIED):

```
gcc -Wall -Wextra -Werror -O2 -c sections.c -o sections.o      # exit 0
gcc -Wall -Wextra -Werror -O2 -c libgeom.c -o libgeom.o        # exit 0
gcc -Wall -Wextra -Werror -O2 -c main.c -o main.o              # exit 0
gcc main.o libgeom.o -o good.exe                               # exit 0
./good.exe
# area = 12, global = 100
# exit 0
gcc -Wall -Wextra -Werror -O2 sections.o -o sections_demo.exe  # exit 0
./sections_demo.exe
# hello, elf world 82 8
# exit 0
nm libgeom.o    # T geom_area, b calls  (the static counter is not exported)
nm main.o       # U geom_area, T main, D global_from_main
```

`sections.c` is a standalone translation unit with its own `main`; it demonstrates the
section layout when inspected with `objdump -h`/`nm`, and it also runs on its own.

## Target build on ELF (documented-as-target; needs Linux or WSL)

```
gcc -Wall -Wextra -Werror -O2 -c sections.c -o sections.o
readelf -h sections.o    # EI_CLASS=2 (64-bit), EI_DATA=1 (LE), e_type=ET_REL
readelf -S sections.o    # .text/.data/.bss (SHT_NOBITS)/.rodata/.symtab/.rela.text
readelf -s sections.o    # GLOBAL func / LOCAL static / UND printf
readelf -r sections.o    # R_X86_64_PLT32 printf, R_X86_64_PC32 on local calls
gcc main.o libgeom.o -o good
```

## What this demonstrates

- Common sections map to C storage classes: functions → `.text`, initialized globals →
  `.data`, uninitialized → `.bss` (NOBITS), constants → `.rodata`.
- `static` produces local (`STB_LOCAL`) symbols; `extern` calls produce undefined (`U`)
  symbols resolved by the linker.
- Object-file code is pre-link: relocation records, not final addresses.
