---
name: zig-inline-asm-and-abi
description: Use when writing or reviewing Zig inline assembly, global asm, calling conventions, @export/@extern, and ABI-sensitive syscall wrappers. Prevents missing or wrong clobbers (unchecked Illegal Behavior), stale stringly-typed clobbers, deleted volatile, and wrong callconv. Version-pinned to Zig 0.15-0.17.
---

# Zig Inline Assembly and ABI

## When to use

- Writing syscall wrappers, CPU-feature probes, or atomic/primitive helpers that need
  exact machine instructions.
- Reviewing `asm` blocks for missing clobbers, wrong constraints, or deleted `volatile`.
- Choosing a calling convention for `extern`/`export` functions and checking ABI
  assumptions (SysV x86-64).
- Linking against symbols via `@extern`/`@export` and global assembly.

## When not to use

- Reading/disassembling assembly generally — see `asm-x86-64-registers-and-addressing`
  and `asm-optimizer-artifacts`.
- C compiler inline asm (`__asm__`) — different constraint syntax; see
  `asm-inline-asm-constraints`.
- Portable CPU detection — prefer `@import("builtin").cpu.arch` and std.Target features.
- FFI with C libraries — use `extern fn` + callconv(.c); see `zig-ffi-c-interop`.

## What the agent often gets wrong

- Omitting `volatile` and later "fixing" deleted asm by re-adding it at -O0 only.
- Declaring clobbers as strings (`: "rcx", "r11"`) — 0.15 switched to typed clobbers
  `: .{ .rcx = true, .r11 = true }`.
- Forgetting to declare registers the asm trashes: the langref says failure to declare the
  full clobber set is *unchecked Illegal Behavior* — it compiles and corrupts.
- Declaring only the input/output registers, or the "memory" clobber when arbitrary memory
  is written.
- Assuming Intel syntax on x86 — Zig inline asm is AT&T (`lea (%rdi,%rsi,1),%eax`).
- Using the wrong calling convention name: `.x86_64`/`.win64` vs the 0.16 `.sysv`/
  `.winapi` naming — check the pinned langref.
- Using `callconv` on a function body that must match the OS ABI at `_start`, or exporting
  without `callconv(.c)`.

## How to reason correctly

1. Ask: can the compiler remove this block? If it has side effects (syscall, memory
   write), add `volatile`.
2. Enumerate every register the block reads or writes: inputs and outputs get constraints;
   everything else the block clobbers (e.g. `rcx`, `r11` across `syscall`) is a clobber.
   If the block writes arbitrary memory, add the `"memory"` clobber.
3. Use typed clobbers (0.15+): `: .{ .rcx = true, .r11 = true }`. On 0.14- era the
   syntax was the string list `"rcx", "r11"`.
4. Write AT&T operand order on x86: `lea (%rdi,%rsi,1),%eax` = `eax = rdi + rsi`.
5. Match the calling convention to the ABI you are talking to: SysV AMD64 args go
   `rdi rsi rdx rcx r8 r9` (sysv-amd64-abi §3.2), return in `rax`; `.c` for C ABI;
   `.winapi`/`.sysv` per target. Use `callconv(.c)` on `export`/`extern` functions.
6. Verify encodings with a disassembler, never by reading the mnemonic.

## What to verify

- `volatile` present wherever the asm has side effects or must not be DCE'd.
- Full clobber set declared; "memory" present for arbitrary writes.
- Typed clobbers syntax on 0.15+; no stringly-typed clobber lists.
- Correct AT&T operand order; `%[name]` template references match constraints.
- Calling conventions match the target ABI; extern symbols link.
- Global assembly (namespace-level `comptime { asm (...) }`) needs no `volatile`, inputs,
  or outputs.

## How to verify

```
zig test examples/good/syscall.zig -target x86_64-linux
zig test examples/good/global_asm.zig -target x86_64-linux -fllvm
zig test examples/good/export_extern.zig
zig test examples/bad/missing_clobber.zig      # compiles but must be flagged (UB)
zig test examples/bad/string_clobbers.zig       # fails on 0.15+: typed clobbers required
zig build-obj examples/good/export_extern.zig
objdump -dr export_extern.o                     # verify symbols and encodings
```

Researched — zig not installed on this host; commands are the recorded verification plan.
`objdump` is provided by binutils (binutils-docs).

## Where the knowledge comes from

- zig-langref §Assembly (Output/Input Constraints, Clobbers, Global Assembly), §Functions
  (callconv, export/extern), §Builtin Functions (@export, @extern).
- zig-release-notes 0.15.x "Inline Assembly: Typed Clobbers".
- sysv-amd64-abi §3.2 (function calling sequence: argument registers, stack alignment,
  return value), §3.2.1 (register roles).
- binutils-docs (objdump/nm for symbol and encoding verification).

## Related skills

- `asm-calling-conventions` — SysV register roles behind Zig's `callconv(.c)`.
- `zig-ffi-c-interop` — `extern fn` / `callconv(.c)` / `@export` for C boundaries.
- `zig-cross-compilation-targets` — ABI and calling conventions vary per target triple.
- `zig-error-model-and-defers` — unchecked Illegal Behavior from missing clobbers.
- `zig-version-migration` — typed clobbers was a 0.15 breaking change.

## Evaluation

- Synthetic: missing-clobber syscall, stringly-typed clobbers on 0.15+, missing
  "memory" clobber, non-volatile side-effecting asm, wrong operand order — must be caught;
  good syscall/global-asm/export examples must pass.
- False-positive: correct `volatile` syscall with typed `.{ .rcx = true, .r11 = true }`
  clobbers, and `callconv(.c)` exports must NOT be flagged.
- Historical: typed clobbers (0.15.0) is the regression target; string clobber lists are
  the pre-0.15 pattern.
- Adversarial: a syscall wrapper that compiles cleanly but corrupts `rcx`/`r11` — the
  reviewer must recognize unchecked Illegal Behavior, not an assembler error.
- Commands and recorded results: `evals/README.md`.
