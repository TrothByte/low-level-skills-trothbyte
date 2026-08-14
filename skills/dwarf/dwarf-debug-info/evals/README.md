# Evaluation — dwarf-debug-info

Skill: `skills/dwarf/dwarf-debug-info`. Stability target: `evaluated`.
Toolchain used for verification: GCC 16.1.0 (MSYS2/MinGW UCRT64), binutils
2.46 (objdump/readelf), gdb 17.2. Host: Windows (PE/COFF `pei-x86-64`).
`llvm-dwarfdump` is NOT installed on this host (documented as target tool).

## Verified facts (recorded output)

F1. `-g` on this toolchain emits DWARF v5 by default: CU header `Version: 5`,
    sections `.debug_loclists`, `.debug_rnglists`, `.debug_line_str` present.
    Sections observed via `objdump -h`: `.debug_info 0xafe9`,
    `.debug_abbrev 0x20af`, `.debug_line 0x1fef`, `.debug_str 0x2f6`,
    `.debug_line_str 0x1a1d`, `.debug_loclists 0x15a1`, `.debug_rnglists 0x168`
    (good binary, `-g -O0`).
F2. `-O0` CU producer: `GNU C23 16.1.0 -mtune=generic -march=nocona -g -O0`;
    `-O2 -g` CU producer: same with `-g -O2`. CU has `DW_AT_name` via
    `(indirect line string ...)` (DW_FORM_line_strp -> `.debug_line_str`) and
    `DW_AT_comp_dir`.
F3. `-O0`: every local/parameter DIE carries a static
    `DW_AT_location : 2 byte block: 91 XX (DW_OP_fbreg: N)`; e.g. `add.sum` at
    fbreg -20, `sum_range.total` at -20, `sum_range.i` (child of a
    `DW_TAG_lexical_block`) at -24, `main.r` at -20, `main.p` (type
    `DW_TAG_structure_type point`, byte_size 8, members x=0, y=4) at -36.
F4. `-O2 -g` DIE facts (`objdump --dwarf=info optimized_away.exe`):
    - `main.s` and `main.u` are `DW_TAG_variable` with NO `DW_AT_location`
      (abbrev 10) -> "value optimized out".
    - inlined `use_reg` instance (inside `main`, `DW_TAG_inlined_subroutine`,
      call_line 28): parameter `a` and variable `prod` have NO location.
    - `seed` (volatile) keeps `DW_OP_fbreg: -20`; `folded_sum`/`use_reg`/
      `triple` appear as abstract instances with `DW_AT_inline : 1/3`;
      concrete out-of-line `use_reg` (0x1400014e0, 0xb bytes) and `folded_sum`
      (0x140001490) keep locations.
    - concrete `use_reg.b` = `DW_OP_reg1 (rdx)` (single exprloc);
      `use_reg.a` and `use_reg.prod` use location lists in `.debug_loclists`.
F5. Location lists (`objdump --dwarf=loc`): v5 table with `DWARF version: 5`,
    "base address" entries and location view pairs. `main.argc` list: rcx ->
    rbx -> fbreg-20 -> rbx. `main.argv` and `use_reg.a` end ranges use
    `DW_OP_entry_value: (DW_OP_reg1/2 (...)); DW_OP_stack_value`. Inlined-loop
    `sum`/`i` are reconstructed as computed expressions
    (`DW_OP_breg2 (rcx): -1; DW_OP_stack_value`, `DW_OP_breg0 (rax): 1; ...`).
F6. Line tables (`objdump --dwarf=decodedline`): `-O0` maps
    `debug_friendly.c:8 -> 0x140001490` (add), `:13 -> 0x1400014b2`
    (sum_range), `:32 -> 0x14000151c` (main), `:37 -> 0x14000156c` (return).
    `-O2 -g` maps several statements to one address (e.g. lines 12-14 all
    begin at 0x140001490), the line-table signature of optimized-away code.
F7. gdb 17.2 (batch):
    - good build, breakpoint debug_friendly.c:37: `r = 3`, `s = 55`,
      `p = {x = 3, y = -4}`, `m = 7`, `argv` readable.
    - bad build, breakpoint optimized_away.c:29: `print s` ->
      `$1 = <optimized out>`, `print u` -> `$2 = <optimized out>`,
      `info locals` -> `s = <optimized out>`, `u = <optimized out>`,
      backtrace shows `main (argc=1, argv=<optimized out>)`.
F8. `readelf --debug-dump=info` on a PE file prints nothing (ELF-oriented);
    `objdump -g` prints COFF legacy info, not DWARF. On this host the correct
    commands are `objdump --dwarf=info|abbrev|decodedline|loc|ranges|str`.

## Synthetic evals

- easy/positive: given `objdump --dwarf=info` output of the `-O0` binary,
  find `sum`'s `DW_AT_location` and state it is a frame-relative stack slot.
- easy/negative: given the `-O2 -g` binary, explain why `s` is "optimized
  out" (DIE present, `DW_AT_location` absent).
- medium/positive: map address 0x14000149e to source line 9 of
  debug_friendly.c using the decoded line table.
- medium/negative: read `use_reg.a`'s location list and identify the
  `DW_OP_entry_value` range; say for which PC it is reconstructable.
- hard/negative: explain that the v5 binary has `.debug_loclists`, not
  `.debug_loc`, and that `sum` in the inlined loop is described by computed
  expressions with `DW_OP_stack_value`.
- adversarial: an agent is shown the inlined `use_reg` instance where `prod`
  has no location but the concrete instance does; it must not claim the
  debug info is corrupted, and must explain the abstract/concrete split.

## False-positive evals (correct behavior must not be flagged)

- A `-O0` variable with `DW_OP_fbreg` location: must NOT be called optimized
  out.
- A DWARF v5 binary missing `.debug_loc`/`.debug_ranges`: must NOT be reported
  as "no debug info".
- A location list with a gap in the middle: the variable is unavailable ONLY
  in the gap; do not declare the whole list broken.
- `-O2 -g` with a readable concrete out-of-line function: do NOT claim all
  variables in the binary are optimized out.

## Verification commands

```
# build (examples/good = -g -O0, examples/bad = -O2 -g)
gcc -g -O0 -o debug_friendly.exe examples/good/debug_friendly.c
gcc -O2 -g -o optimized_away.exe examples/bad/optimized_away.c

# DWARF inspection (works on PE/COFF and ELF)
objdump --dwarf=info optimized_away.exe     # DIE tree; look for DW_AT_location
objdump --dwarf=decodedline debug_friendly.exe  # address -> line mapping
objdump --dwarf=loc optimized_away.exe      # v5 .debug_loclists, DW_OP_entry_value
objdump --dwarf=abbrev optimized_away.exe   # abbrev 10 = variable w/o location
objdump --dwarf=ranges optimized_away.exe   # v5 .debug_rnglists

# gdb (installed: 17.2)
gdb -batch -ex "break optimized_away.c:29" -ex "run" -ex "print s" -ex "print u" optimized_away.exe
# expected: $1 = <optimized out>; $2 = <optimized out>
gdb -batch -ex "break debug_friendly.c:37" -ex "run" -ex "info locals" debug_friendly.exe
# expected: r = 3, s = 55, p = {x = 3, y = -4}, m = 7
```

Target verification (documented, not run on this host): `readelf
--debug-dump=info` on ELF builds; `llvm-dwarfdump --debug-info` (LLVM toolchain);
lldb `frame variable`.

## Scoring (for routing eval)

- precision: every "optimized out" claim must be traceable to a DIE with no
  `DW_AT_location` (or a list gap) in the shown DWARF.
- recall: agent must identify that `s`, `u`, `argv` and inlined `prod` are
  optimized out in the bad binary, and that all locals are available in the
  good binary.
- FP-rate: correct `-O0`/v5 handling and per-range gap reasoning must produce
  zero false flags.
