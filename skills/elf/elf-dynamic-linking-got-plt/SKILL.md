---
name: elf-dynamic-linking-got-plt
description: Use when explaining or debugging how a dynamically linked ELF program resolves external calls and data — GOT/PLT layout, lazy vs eager binding (LD_BIND_NOW), R_X86_64_GOTPCREL/PLT32 relocations, readelf -d and objdump -R output, symbol interposition, and -fPIC/-fPIE implications.
---

# Dynamic Linking: GOT/PLT & Lazy Binding

## When to use

- Explaining why an external call is indirect: `call *rel(%rip)` through a PLT/GOT or an
  IAT slot, in `objdump -d`/gdb output.
- Answering "when does this undefined symbol fail?" — at load (data, or `LD_BIND_NOW`)
  vs at first call (lazy function binding).
- Reading `readelf -d` (NEEDED, JMPREL, RELA, BIND_NOW/FLAGS) or `objdump -R`
  (JUMP_SLOT, GLOB_DAT, RELATIVE) on a linked binary.
- Explaining symbol interposition: why a `.so`'s internal call resolves to the
  executable's function under `-rdynamic`, and why it does not by default.
- Deciding between `-fPIC` and `-fPIE`, or explaining why imported data still goes
  through the GOT.
- Prerequisite/extension work for `elf-linker-loader-debugger` on the load stage.

## When not to use

- Windows PE/COFF internals as the *subject*: PE has no lazy binding and no symbol
  interposition by default; use this skill's PE analog only as a host-verifiable
  cross-check, not as the target behavior.
- Static linking, linker scripts, bare-metal images (no loader, no GOT/PLT) — use
  `embedded-linker-script`.
- Object-file relocation reading (`readelf -r` on `.o`, `-fPIC` link errors) — use
  `elf-layout-and-relocations`.
- The full load/run pipeline, `LD_LIBRARY_PATH` search order, `_start` startup sequence —
  use `elf-linker-loader-debugger`.

## What the agent often gets wrong

- "The GOT already contains the final addresses" — the loader fills it from the dynamic
  relocation tables; lazily-bound function slots are patched on first call.
- "PLT entries contain real code addresses" — a PLT entry is a stub that indirects
  through a GOT slot.
- "An undefined function fails at load" — with lazy binding it fails at the first call;
  only data (and everything under `LD_BIND_NOW`) fails at load.
- "`LD_BIND_NOW` is a startup optimization" — it is a binding-strategy switch whose real
  observable is *when* resolution and failures happen.
- "Interposition always happens / never happens" — it depends on the executable's
  `.dynsym` contents (`-rdynamic`), scope order, and visibility.
- "`objdump -R` shows relocations in the object file" — `-R` is the loader's dynamic
  relocation list in the final image; objects use `-r`.
- "With `-fPIC` every call goes through the PLT" — the linker relaxes `PLT32` to direct
  `PC32` for non-preemptible symbols.
- Importing ELF claims onto the host: PE resolves imports eagerly at load, `objdump -R`
  errors on PE, and PE executables do not interpose over DLL internals.

## How to reason correctly

1. Classify the reference: external function → PLT (`R_X86_64_PLT32` in the object,
   `JUMP_SLOT` in the image); external data → GOT (`GOTPCREL` in the object,
   `GLOB_DAT` in the image). Non-preemptible calls relax to direct `PC32`.
2. Classify the resolution time: lazy (default) resolves function slots on first call;
   `LD_BIND_NOW`/`-z now` resolves everything at load. Then predict the failure phase.
3. Ask "who owns the symbol and what is in `.dynsym`": `STB_LOCAL`/hidden → no dynamic
   machinery; default-visibility exported global → preemptible → PLT/GOT;
   `STV_PROTECTED` or `-Bsymbolic` → bound locally.
4. Read the tables with the right tool at the right stage: `readelf -d` (linker-emitted
   dynamic section), `objdump -R` (loader's relocation worklist), `nm -D` (actual export
   set), `LD_DEBUG=bindings` (live binding decisions).
5. Verify with an observable, not an assumption: GOT slot value before vs after the
   first call, or a load-time failure under `LD_BIND_NOW` that lazy binding does not
   reproduce.

## What to verify

- `readelf -d`: `NEEDED`, `SONAME`, `PLTGOT`, `JMPREL`/`PLTRELSZ`, `RELA`/`RELASZ`,
  `FLAGS`/`FLAGS_1` (NOW bit) to decide lazy vs eager.
- `objdump -R`: presence and counts of `R_X86_64_RELATIVE`, `R_X86_64_GLOB_DAT`,
  `R_X86_64_JUMP_SLOT`; JUMP_SLOT count consistent with `readelf -d`.
- PLT stub disassembly before and after the first call (GOT slot target changes from the
  resolver path to the library code).
- Lazy vs eager failure timing: missing function fails at first call vs at load.
- Interposition: `nm -D` of exe and `.so`; binding direction under `LD_DEBUG=bindings`
  with and without `-rdynamic`.
- On this host (PE/COFF): DLL export table contents, import thunk stubs and indirect
  calls, `objdump -p` imports; `objdump -R` failure on PE.

## How to verify

On an ELF host (Linux or WSL), requires `readelf`, `objdump`:

```
gcc -fPIC -shared -o libcounter.so libcounter.c
gcc -pie -fPIE main.c -L. -lcounter -Wl,-rpath,'$ORIGIN' -o prog
readelf -d prog              # NEEDED libcounter.so, JMPREL, RELA, no BIND_NOW
objdump -R prog              # R_X86_64_RELATIVE, GLOB_DAT, JUMP_SLOT
objdump -d prog | grep '@plt'   # PLT stubs: jmp *rel(%rip); push idx; jmp .plt
nm -D libcounter.so          # exported: counter_bump counter_get counter_reset
LD_DEBUG=bindings ./prog     # first call to counter_bump binds; later calls do not
LD_BIND_NOW=1 ./prog         # all bindings at load
gcc -z now -o prog_now prog.o -L. -lcounter -Wl,-rpath,'$ORIGIN'
readelf -d prog_now          # FLAGS/FLAGS_1: NOW, no lazy path
```

On this repository's host (MinGW gcc 16.1, PE/COFF) — VERIFIED commands:

```
gcc -Wall -Wextra -Werror -O2 -DLIBCOUNTER_BUILD -c libcounter.c -o libcounter.o
gcc -Wall -Wextra -Werror -O2 -c main.c -o main.o
gcc -shared -o libcounter.dll libcounter.o -Wl,--out-implib=libcounter.dll.a
gcc main.o -L. -lcounter -o good_dyn.exe && ./good_dyn.exe   # counter = 3, exit 0
objdump -p libcounter.dll     # export table: counter_bump/counter_get/counter_reset
objdump -p good_dyn.exe       # imports: DLL Name: libcounter.dll
objdump -d good_dyn.exe       # jmp *rel(%rip) # __imp_counter_*  (import thunk)
objdump -R good_dyn.exe       # errors: "not a dynamic object"  (VERIFIED)
```

ELF GOT/PLT specifics (lazy binding via `_dl_runtime_resolve`, `objdump -R` JUMP_SLOT,
`LD_BIND_NOW` timing, interposition) are documented-as-target; do NOT claim they were
run on this host.

## Where the knowledge comes from

- System V ABI — ELF: dynamic section, GOT/PLT, symbol binding/visibility, dynamic
  relocations (`sysv-elf`).
- System V AMD64 psABI: `R_X86_64_GOTPCREL`/`R_X86_64_PLT32` formulas,
  position-independent code, preemption (`sysv-amd64-abi`).
- GNU binutils documentation: `readelf -d/-R`, `objdump -R/-p` semantics
  (`binutils-docs`).
- GDB User Manual: observing resolver calls and memory to confirm binding
  (`gdb-manual`).

## Related skills

- `elf-layout-and-relocations` — prerequisite: sections, symbols, relocation types
  (require of).
- `elf-linker-loader-debugger` — the load/run pipeline this skill's mechanisms belong
  to (recommend to).
- `embedded-linker-script` — the no-loader case where GOT/PLT are absent.
- `abi-layout-reasoning` — call-site and data-access ABI context.

## Evaluation

Synthetic: given `readelf -d`/`objdump -R` excerpts, name each dynamic entry and
relocation class and decide lazy vs eager; compute `PLT32`/`GOTPCREL` results; predict
failure phase (load vs first call) for missing function/data under both binding modes;
given `nm -D` + link flags, predict whether interposition occurs. Adversarial: a
`-rdynamic` executable and a `.so` both defining `helper` — explain which `helper` the
library's internal call reaches. False-positive: correct PIC/PIE code, default lazy
binding, non-preemptible `PLT32` relaxed to `PC32`, and PE/COFF builds without any
lazy-binding expectation must NOT be flagged. See `evals/README.md`.
