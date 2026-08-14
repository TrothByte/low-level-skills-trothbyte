---
name: dwarf-debug-info
description: Use when reading or generating DWARF debug info, mapping addresses to source lines, explaining "value optimized out" in optimized builds, writing debug-friendly code, or inspecting binaries with objdump/readelf/gdb. Teaches DWARF sections, DIEs, attributes, location lists, and optimized-code debugging strategies.
---

# DWARF Debug Info: Reading, Reasoning, and Debugging Optimized Code

## When to use

- Explaining "value optimized out" from gdb/lldb in a `-O2` build.
- Mapping an address (crash log, backtrace) to a source line.
- Inspecting debug info: `objdump --dwarf=*`, `readelf --debug-dump=*`,
  `llvm-dwarfdump`.
- Choosing `-g/-Og/-O0` for debugger-friendly builds.
- Reading tool output mentioning DIEs, tags, attributes, location lists,
  `DW_OP_*`, `.debug_info`, `.debug_line`, `.debug_loclists`.

## When not to use

- Reading assembly/optimizer output — use `asm-optimizer-artifacts`.
- ELF/PE layout, linking, symbols — use `elf-linker-loader-debugger`.
- Calling conventions, struct layout — use `abi-layout-reasoning`.
- PDB/CodeView (MSVC) — different format, different tools.

## What the agent often gets wrong

- "`-g` is missing, so variables are optimized out." The DIE exists; it just
  has no `DW_AT_location` for that PC range.
- "The debugger is buggy." Absent `DW_AT_location` is a compiler decision.
- "Variables have one fixed location." At `-O2` they move between registers and
  stack; location lists encode per-range homes, with gaps.
- "Line numbers come from the symbol table." They come from `.debug_line`
  (a state-machine program), read via `objdump --dwarf=decodedline`.
- "v4 and v5 sections are the same." v5 renamed `.debug_loc`→`.debug_loclists`,
  `.debug_ranges`→`.debug_rnglists`, added `.debug_line_str`.
- "`DW_AT_location` is a memory address." It is a `DW_OP_*` expression:
  register, frame offset, or computed value (`DW_OP_stack_value`).
- "`readelf --debug-dump` works everywhere." On PE/COFF (MinGW) it prints
  nothing; use `objdump --dwarf=info` there.

## How to reason correctly

1. Check the build (`-O0`/`-Og` vs `-O2`) and the CU `Version:` (v4 vs v5).
2. Pick the section: DIEs in `.debug_info`, schema in `.debug_abbrev`, lines
   in `.debug_line`, locations in `.debug_loclists`/`.debug_loc`.
3. Inspect the variable DIE: no `DW_AT_location` → optimized out; a
   `(location list)` → read per-range expressions.
4. Read `DW_OP_*`: `DW_OP_fbreg` = frame-relative slot, `DW_OP_regN` =
   register, `DW_OP_stack_value` = computed, `DW_OP_entry_value` = entry state.
5. For inlined code follow `DW_AT_abstract_origin` to the abstract instance;
   concrete instances carry real locations.
6. Confirm in gdb: `print var` / `info locals` must agree with the DWARF.

## What to verify

- Availability claims are backed by `DW_AT_location` (or its absence) in the DIE.
- Address → line claims match `objdump --dwarf=decodedline`.
- Code you call "debuggable" yields values in gdb, not "optimized out".
- Section names match the CU header's DWARF version.

## How to verify

```
gcc -g -O0 -o good.exe examples/good/debug_friendly.c
gcc -O2 -g -o bad.exe examples/bad/optimized_away.c
objdump --dwarf=info bad.exe          # DIEs, DW_AT_location presence
objdump --dwarf=decodedline good.exe  # address -> line mapping
objdump --dwarf=loc bad.exe           # location lists (v5 .debug_loclists)
gdb -batch -ex "break optimized_away.c:29" -ex "run" -ex "print s" bad.exe
```

## Where the knowledge comes from

- DWARF Debugging Information Format v5 (sections, DIEs, locations, line tables)
- GDB User Manual — Optimized Code Debugging
- GNU Binutils docs — objdump/readelf `--dwarf`/`--debug-dump` options
- GCC Manual — `-g`, `-Og`, `-O*` debug-info behavior
- System V AMD64 ABI §6 (DWARF in ELF); verified empirically on this toolchain

## Related skills

- `elf-linker-loader-debugger` — requires this skill to read the debug data (require of)
- `asm-optimizer-artifacts` — reading the code side of the same binaries (recommend)
- `abi-layout-reasoning` — frame layout behind `DW_OP_fbreg`/`DW_OP_reg`

## Evaluation

Synthetic: explain why `main`'s `s`/`u` are optimized out in
`examples/bad/optimized_away.c` (-O2 -g) but readable in
`examples/good/debug_friendly.c` (-g -O0); find `sum`'s `DW_AT_location` in
each; map an address to a line via the decoded line table.
False-positive: a `-O0` variable with `DW_OP_fbreg` must NOT be called
optimized out; a v5 binary must NOT be reported as "no `.debug_loc`".
Adversarial: a location list with `DW_OP_entry_value` must be read correctly;
a value with a list gap is unavailable only in the gap.
