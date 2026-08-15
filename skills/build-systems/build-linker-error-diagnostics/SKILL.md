---
name: build-linker-error-diagnostics
description: Use when linking fails: undefined references, symbol/ABI mismatches, archive pull-in and --whole-archive behavior, wrong-mangled or versioned symbols, or massive undefined-ref cascades. Teaches reading symbol tables (nm/objdump/readelf) instead of guessing which flag to add.
---

# Linker Error Diagnostics: Read the Symbol Table, Not the Error Count

## When to use

- `undefined reference to ...` at link time, single or by the thousand.
- A symbol "exists" (same name as the error) but the link still fails.
- Static archives: a member symbol that "should" link is not pulled in.
- `--whole-archive`, `--as-needed`, symbol order/duplicate definitions.
- A huge undefined-ref cascade that looks like "the linker broke".

## When not to use

- Configure-stage CMake dependency errors — use `build-system-cmake-diagnostics`.
- `-std=` / compiler version mismatch — use `build-toolchain-version-drift`.
- Corrupted build state / signal-killed builds — use
  `build-process-signal-and-state-safety`.

## What the agent often gets wrong

- Reading "undefined reference" as "write more code" instead of asking which
  object actually defines the symbol.
- "The symbol exists in the source, so the linker is wrong" — the linker works
  on the symbol table, and C++ mangling means `add(int)` and `add(double)` are
  different symbols.
- Fixing a 40k-error cascade one error at a time; the count is irrelevant, the
  shared pattern (one missing generated table) is the cause.
- Adding random `-l` flags or reordering libraries to silence the error.
- Not knowing that archives pull members lazily: an unreferenced symbol in an
  archive member is not linked unless `--whole-archive` (or a reference) forces
  it.

## How to reason correctly

1. Every link is a symbol-satisfaction problem. Enumerate the unsatisfied
   symbols first: `nm -u` / `objdump -t` (UND / `sec 0` rows).
2. For each, find where a definition *could* live: `nm -C lib.o` for objects,
   `nm lib.a` for archives, `objdump -t` on the final image. A definition with
   the wrong mangled/versioned name does not satisfy the reference.
3. Classify: undefined-but-not-defined (missing object/library), wrong
   mangled/versioned name (C++ signature or ABI version), or not-pulled
   (archive member never referenced; `--whole-archive`).
4. For cascades: read the FIRST error and the name prefix. `__ksym_N`/
   `__jump_table`-style families mean one missing generated definition.
5. Verify the fix with the same tools: after linking, `nm` on the executable
   shows the symbol as DEFINED, and the archive member count matches intent.

## What to verify

- The exact `undefined reference to 'X'` error text and the `collect2`/`ld`
  exit code (1).
- `nm`/`objdump -t` on objects: the symbol is `U` / `sec 0` (undefined) before
  the fix and `T` (defined in text) after.
- For C++: the error's demangled name vs `nm -C`'s list — the mismatch is the
  cause.
- For archives: `nm` on the executable lists the pulled members; `--whole-archive`
  pulls all, default pulls only referenced.
- The cascade: all errors share one prefix; resolving the single source clears
  them all.

## How to verify

```
gcc -c bad/undef_main.c -o undef.o
nm undef.o                 # U missing_func  (or: objdump -t undef.o)
gcc bad/undef_main.c       # undefined reference to `missing_func'; exit 1
gcc good/main.c good/lib.c # exit 0; run exit 0

gcc -c bad/mismatch_lib.cpp -o lib.o
nm -C lib.o                # T add(double)   (mangled: _Z3addd)
gcc bad/mismatch_app.cpp bad/mismatch_lib.cpp
                           # undefined reference to `add(int)'; exit 1

ar rcs libmulti.a foo.o bar.o
gcc good/arch_main.c libmulti.a -o app.exe
nm app.exe                 # only foo present
gcc good/arch_main.c -Wl,--whole-archive libmulti.a -Wl,--no-whole-archive -o w.exe
nm w.exe                   # foo and bar both present
```

## Where the knowledge comes from

- `gnu-ld-manual` — archive member extraction, --whole-archive, --as-needed.
- `lld-docs` — the same semantics for LLD; LTO/ODR diagnostics.
- `binutils-docs` — nm, objdump -t, readelf -s semantics.
- `sysv-elf` — ELF symbol tables, UND/TYPE fields, versioned symbols.

## Related skills

- `build-system-cmake-diagnostics` — dependency misdeclaration before linking
- `build-toolchain-version-drift` — ABI/version drift behind symbol mismatches
- `elf-layout-and-relocations` — relocation resolution behind the errors
- `elf-dynamic-linking-got-plt` — dynamic symbol resolution for shared libs

## Evaluation

- Synthetic: bad examples must fail with the recorded `undefined reference`
  errors (exit 1); good examples must link and run (exit 0).
- False-positive: a defined symbol with a different signature must be diagnosed
  as a signature/mangle mismatch, NOT as "missing definition"; a lazily-pulled
  archive member that is intentionally unreferenced must NOT be flagged.
- Historical: replay the 40,784-ref kernel cascade — correct output is "one
  missing generated definition (__jump_table/__ksymtab)", not 40k fixes.
- Adversarial: a "fix" that adds `-lfoo` or reorders libraries without checking
  `nm` must be rejected unless the symbol becomes DEFINED.
- Verified facts (actual runs recorded 2026-08-15): `evals/README.md`.
