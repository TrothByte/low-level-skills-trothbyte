---
name: elf-linker-loader-debugger
description: Use when diagnosing or building ELF binaries — symbol resolution, static vs dynamic linking, relocation failures, undefined symbols, missing -fPIC, PLT/GOT lazy binding, .init_array ordering, _start-to-main flow, dynamic loader errors, or mapping addresses to source lines in a debugger. Explains the compiler-to-linker-to-loader-to-debugger pipeline as one process.
---

# ELF + Linker + Loader + Debugger as One Pipeline

## When to use

- Diagnosing link errors: `undefined reference`, `multiple definition`, "cannot find -lfoo".
- Distinguishing a link-time failure from a load-time failure (`cannot open shared object`, `undefined symbol` at startup).
- Building or debugging shared libraries (`.so` / DLL) — `-fPIC`, exports, imports, symbol interposition.
- Explaining how a call to an external function actually executes (relocation → PLT/GOT → lazy binding).
- Tracing what runs before `main` (`.init_array`, constructors) or after (`_fini`).
- Mapping a crash address or a disassembly address back to a source line in a debugger.
- Reviewing build/run logs where the compiler, linker, loader, or debugger all mention the same symbol.

## When not to use

- PE/COFF-only questions about Windows internals (import libs, `.pdata`); this skill covers PE only as a cross-check host.
- DWARF internals in depth — use `dwarf-debug-info`.
- ELF layout fundamentals (section names, `e_*` fields) — use `elf-layout-and-relocations`.
- Assembly instruction semantics — use `asm-x86-64-registers-and-addressing`.
- C-level UB in the linked program — use `c-undefined-behavior`.

## What the agent often gets wrong

- "The symbol exists in the source, so the link error is a compiler bug." The compiler emits a relocation; the LINKER resolves it. A name mismatch, a missing file, or a missing `-l` produces `undefined reference` no matter how correct the C looks.
- "Unresolved symbol always fails at link time." On ELF, an undefined symbol inside a shared object can survive both links and only fail at runtime — load time for data, first call for functions under lazy binding.
- "`-fPIC` is a performance detail." On x86-64 ELF it is a correctness requirement: non-PIC code can embed 32-bit absolute relocations the linker rejects when making a shared object.
- "`main` is the first code that runs." The dynamic loader, `.preinit_array`, `.init_array`, and libc startup (`_start` → `__libc_start_main`) all run first, in a defined order.
- "All symbols in a library are visible to the executable." Only symbols that survive into `.dynsym` with default visibility are exported.
- "A debugger shows source lines by magic." It walks DWARF `.debug_line`/`.debug_info`; stripped or optimized binaries lose or distort that mapping.

## How to reason correctly

1. Trace the symbol through each stage: source → object (relocation entry) → linked image (patched address) → runtime image (load-time fixup) → debugger (DWARF lookup).
2. Classify every error by phase: compile, link (`ld`/`collect2`), load (`ld.so`/Windows loader), execution.
3. For an `undefined reference`: ask where the definition is, whether it was compiled, whether the object/archive/DSO was on the link line, and whether the symbol is exported and visible.
4. For a multiple definition: check duplicate object files and `-Wl,--no-whole-archive` style mistakes before suspecting the compiler.
5. For dynamic binaries, predict the failure stage by symbol kind (function vs data) and binding mode (lazy vs `-z now`).
6. Verify against the actual artifact with `nm`, `objdump`/`readelf`, and the debugger — never assume from source alone.

## What to verify

- Clean build under `-Wall -Wextra -Werror -O2`.
- The symbol table of the object files (`nm`): expected `U` (undefined) and `T`/`D` (defined) entries.
- The relocation records in the object and the final image (`objdump -r` / `readelf -r`).
- On ELF: readelf output for `-h` (header), `-S` (sections), `-l` (segments), `-s` (symbols), `-d` (dynamic), `-r` (relocations).
- Runtime behavior: does the failure appear at load or after start?
- DWARF mapping: breakpoints, `info line`, backtraces resolve to correct source lines.

## How to verify

Verified on this repository's host (MinGW gcc 16.1 / binutils 2.46, PE/COFF; DWARF debug info):

```
gcc -Wall -Wextra -Werror -O2 -c libfoo.c -o libfoo.o
gcc -Wall -Wextra -Werror -O2 -c main.c -o main.o
gcc main.o libfoo.o -o prog        # static link; run exits 0, prints expected output
gcc -shared -o libfoo.dll libfoo.o -Wl,--out-implib,libfoo.dll.a
gcc main.o -L. -lfoo -o prog_dyn   # dynamic link against import lib
nm main.o && nm libfoo.o           # U foo_add / T foo_add
objdump -r main.o                  # IMAGE_REL_AMD64_REL32 foo_add
objdump -p prog_dyn.exe            # "DLL Name: libfoo.dll" import table
gdb -batch -ex "break main" -ex run -ex "info line main" prog
```

Target verification — requires an ELF host (Linux or WSL), not run here:

```
gcc -fPIC -shared -fvisibility=hidden -o libfoo.so libfoo.c
gcc main.c -L. -lfoo -o prog       # or LD_LIBRARY_PATH=. ./prog
readelf -h -S -l -s -d -r prog
objdump -R prog                    # relocation table incl. JUMP_SLOT entries
LD_DEBUG=symbols LD_BIND_NOW ./prog
gdb -batch -ex "break main" -ex run -ex "info line main" -ex bt prog
```

## Where the knowledge comes from

- System V ABI — ELF (generic) and x86-64 psABI: ELF header, sections/segments, symbol table, relocations, dynamic section, PLT/GOT, process start.
- GNU binutils documentation and GNU ld manual: object inspection, linker options (`-z now`, `-z defs`, `--gc-sections`, `--as-needed`).
- DWARF v5 standard: `.debug_line`, `.debug_info`, location lists.
- GDB manual: breakpoints, `info line`, optimized-code debugging.

## Related skills

- `elf-layout-and-relocations` — required prerequisite (section/segment/symbol basics).
- `dwarf-debug-info` — required prerequisite (DWARF internals).
- `elf-dynamic-linking-got-plt` — deeper GOT/PLT mechanics.
- `embedded-linker-script` — consumes this skill (linker scripts for bare metal).
- `abi-layout-reasoning`, `asm-x86-64-registers-and-addressing` — adjacent ABI/asm context.

## Evaluation

Synthetic: correct extern linkage must build and run; symbol mismatch must be detected as a link-time `undefined reference`; multiple definition must be detected; unresolved symbol in a shared object must be classified by phase (link vs load); missing `-fPIC` must be flagged as an ELF-only link failure. Adversarial: lazy-binding predictions (which call fails first, and when); `.init_array` ordering. False-positive: a correct library build (including non-static library symbols that are intentionally exported) must not be flagged, and MinGW builds must not be told to add `-fPIC`.

See `evals/README.md` for cases, commands, and the verified-facts list.
