---
name: go-rust-re
description: Use when analyzing or reverse-engineering Go or Rust binaries — recovering function names from .gopclntab, decoding mangled Rust symbols (_ZN/_R), finding panic strings and core::fmt literals in .rodata/.rdata, distinguishing Rust from C/C++, or triaging stripped Go/Rust executables with GoReSym, redress, objdump, gdb, and Delve.
---

# Go and Rust Binary Reverse Engineering

## When to use

- Triaging a binary that may be written in Go or Rust (malware, crash dumps, CTF, third-party CLI tools).
- Recovering function names from a stripped Go binary (`.gopclntab` still holds them).
- Reading mangled Rust symbols (`_ZN...` legacy vs `_R...` v0) or reconstructing `dyn Trait` dispatch.
- Explaining a Rust panic path from a binary: `rust_begin_unwind`, `core::panicking::panic_fmt`,
  OOB-index and `unwrap()` messages in `.rdata`/`.rodata`.
- Telling Rust, C, C++, and Go apart from symbols, imports, panic strings, and entry flow.
- Planning the GoReSym → IDA/Ghidra → redress recovery pipeline for a Go target.

## When not to use

- General ELF/PE layout, linking, imports — use `elf-linker-loader-debugger` / `elf-layout-and-relocations`.
- DWARF internals, "optimized out" reasoning — use `dwarf-debug-info`.
- Instruction semantics / addressing modes — use `asm-x86-64-registers-and-addressing`.
- Rust panic *reachability* in source (not in the binary) — use `rust-panic-safety`.
- SysV/Windows calling-convention details for C code — use `asm-calling-conventions` / `abi-layout-reasoning`.

## What the agent often gets wrong

- "Stripped Go binary = no function names." `.gopclntab` is runtime metadata independent of `.symtab`; GoReSym
  recovers everything after `strip -s`.
- Treating `.gopclntab` as a plain symbol table and reading it linearly; it is a versioned, PC-indexed table and
  the field layout depends on the magic (Go 1.2/1.16/1.18/1.20 layouts differ).
- "Go strings are C strings." Go strings are (ptr, len), UTF-8, no NUL terminator; NUL-based parsing truncates.
- Applying the SysV AMD64 ABI to Go functions — Go uses ABIInternal (AX, BX, CX, DI, SI, R8, R9), not RDI/RSI/RDX.
- "Rust mangling is always `_ZN...`." VERIFIED on rustc 1.97.1 pe-x86-64: the default is v0 (`_R...`); legacy
  needs nightly there. Confusing Rust `_ZN...17h<hash>E` with C++ `_ZN...Ev`: the hash suffix and `core`/`std`
  crates are the tell.
- "No symbols ⇒ nothing to recover" on a stripped Rust PE. Panic strings, format literals, and source paths are
  data and survive `-C strip=symbols` (VERIFIED).
- Not reading `.go.buildinfo` first — the exact Go version selects every later parsing rule.

## How to reason correctly

1. Triage: identify the language. Go ⇒ `.gopclntab` magic + `.go.buildinfo` + dotted names (`main.main`);
   Rust ⇒ `_R...`/`_ZN...17h...` symbols, `panicked at`/`core::fmt` strings, `lang_start` entry flow; C/C++ ⇒
   `_Z`/`_ZN...Ev` without hash suffixes.
2. Go: read the version first (`.go.buildinfo`, pclntab magic), pick the layout, run GoReSym
   (`-t -d -p -strings`), cross-check addresses with `objdump`; decompile with redress/Ghidra.
3. Rust: collect the mangling scheme, then the panic corpus and `core::fmt` templates from `.rdata`; walk each
   panic string back to its `lea` + `call <panic_fmt>` site.
4. Never decode Go args with SysV rules, never model Go strings as NUL-terminated, and never trust a symbol
   table you have not confirmed survives stripping.

## What to verify

- The language claim is backed by at least two independent markers (e.g. mangling scheme + panic strings).
- Go: pclntab magic matches the `.go.buildinfo` version; recovered function addresses fall inside `.text`.
- Rust: the panic/format strings found in `.rdata` correspond to the reported code paths; the symbol scheme
  matches the toolchain default for the target.
- Any claim about a toolchain not run on this host (Go) is marked `documented-as-target`, not VERIFIED.

## How to verify

Verified on this host (rustc 1.97.1, pe-x86-64, binutils 2.46):

```
rustc -O examples/good/rust_panic_demo.rs -o rpd.exe
rustc -O -C strip=symbols examples/good/rust_panic_demo.rs -o rpd_stripped.exe
rustc -O --emit=obj examples/good/rust_panic_demo.rs -o rpd.obj        # _RNv... symbols here
objdump -t rpd.exe rpd_stripped.exe        # "no symbols" on the final PE (expected, not failure)
strings -a rpd_stripped.exe                # panic corpus survives stripping
objdump -s -j .rdata rpd.exe               # panic/format literals with VMAs
objdump -d rpd.exe                         # lea <string>; call <panic_fmt> at the panic site
objdump -t rpd.obj                         # _RNvCs..._15rust_panic_demo4main, ..._4core9panicking9panic_fmt
objdump -p rpd.exe                         # VCRUNTIME140.dll / api-ms-win-crt-* imports
./rpd.exe -5; ./rpd.exe 2 9; ./rpd.exe 2 1 # panic exit 101 vs normal exit 0
```

Target verification — requires a Go toolchain and an ELF host (documented-as-target):

```
go version -m bin                                   # module path + exact Go version
objdump -s -j .gopclntab bin | head                 # magic 0xfffffff1 for Go 1.20+
GoReSym -t -d -p -strings bin > meta.json           # functions/types/strings from the pclntab
go tool objdump -s 'main\.main' bin                 # ABIInternal register convention
dlv exec bin -- goroutines                          # goroutine stacks, goid
```

## Where the knowledge comes from

- GDB manual — Go language support (string/type rendering); debugging Go programs.
- GNU binutils docs — objdump/readelf/nm/strip/strings for symbol, section, and string inspection.
- DWARF v5 — Go emits DWARF by default (another name/line recovery path unless built with `-w`).
- The Rust Reference — mangling, panicking (`panic_fmt`, `panic_bounds_check`), type layout / trait objects.
- System V AMD64 ABI — the convention that must NOT be applied to Go ABIInternal; ELF symbol semantics.
- Go runtime sources (`src/debug/gosym/pclntab.go`, `src/internal/abi/symtab.go`) and the Go 1.2 symbol-table
  design doc — pclntab layout and magic, mirrored by GoReSym (Mandiant) and redress (goretk). Go items are
  `documented-as-target` on this host.

## Related skills

- `elf-linker-loader-debugger` — symbol/import/PLT context for the same binaries (recommend)
- `dwarf-debug-info` — DWARF-based name/line recovery for Go and debug Rust builds (recommend)
- `asm-x86-64-registers-and-addressing` — reading the `.text` the RE claims are about (require of)
- `asm-calling-conventions` — SysV/Windows conventions to distinguish from Go ABIInternal (recommend)
- `rust-panic-safety` — source-level meaning of the panic paths found in the binary (recommend)
- `abi-layout-reasoning` — struct/vtable layout behind trait objects (recommend)

## Evaluation

Synthetic: identify Go via `.gopclntab` magic + `.go.buildinfo`; recover `main.main` after strip; extract a
Rust panic path from a stripped PE; classify `_ZN...17h...` as Rust, not C++; read a trait-object vtable call
as indirect dispatch. Adversarial: Go 1.16 vs 1.20 pclntab layout misparse; a Rust binary whose only markers
are panic strings; SysV arg decoding applied to a Go function. False-positive: a C++ binary must NOT be flagged
Rust on `_ZN` alone; an intact `.gopclntab` must not be called "stripped and opaque"; panic strings must not be
reported as "no Rust markers". See `evals/README.md` for cases, commands, and verified facts.
