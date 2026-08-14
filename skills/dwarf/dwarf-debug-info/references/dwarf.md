# DWARF Debug Info: Deep Reference

Format per rule: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE →
COUNTEREXAMPLE → VERIFICATION → SOURCE. Verified claims are marked [VERIFIED]
with the toolchain used (GCC 16.1 / MinGW, binutils 2.46 objdump, gdb 17.2).
Normative claims cite DWARF v5 sections; tool behavior cites binutils/gdb/gcc docs.

## 1. Debug info lives in named sections, not in the code

- **RULE**: DWARF is emitted into special sections (ELF `.debug_*`, COFF
  `.debug_*` on MinGW). It is a full separate description of types, variables,
  functions, and line numbers, written at compile time and never touched by
  ordinary code execution.
- **WHY AI GETS IT WRONG**: "the binary knows the variable names because the
  compiler kept symbols" — the symbol table (`.symtab`, `nm`) holds only
  linkage-level names; all type/line/location detail is in the DWARF sections.
- **CORRECT REASONING**: strip the `.debug_*` sections and `gdb` can no longer
  print types or line numbers, while `nm` output is unchanged. Tools that read
  debug info read the DWARF sections, not the code.
- **EXAMPLE**: `objdump -h debug_friendly.exe` shows `.debug_info`,
  `.debug_abbrev`, `.debug_line`, `.debug_line_str`, `.debug_str`,
  `.debug_loclists`, `.debug_rnglists`, `.debug_frame`, `.debug_aranges`
  (all with flag `DEBUGGING`). [VERIFIED: GCC 16.1 MinGW]
- **COUNTEREXAMPLE**: a binary built without `-g` has no `.debug_*` sections at
  all; the linker does not invent them.
- **VERIFICATION**: `objdump -h <file>` and grep for `debug`; compare against a
  `-g`-less build.
- **SOURCE**: `dwarf-v5` §7.1 (debug info sections); `sysv-elf` (ELF section
  layout); `binutils-docs` (objdump -h).

## 2. .debug_info: the DIE forest

- **RULE**: `.debug_info` contains Debugging Information Entries (DIEs) — a
  tree rooted at one `DW_TAG_compile_unit` (or several, one per compiled
  translation unit). Each DIE is `(tag, attribute list, children)`.
- **WHY AI GETS IT WRONG**: "DWARF is a flat table of variables" or "a single
  XML-like blob"; actually it is a tree encoded with compressed numbers and
  cross-referenced by offset (`<0x29f7>` style references).
- **CORRECT REASONING**: the tree mirrors source scopes: compile unit → types →
  subprograms → lexical blocks → variables. A DIE reference `<0x2941>` is an
  offset into the same `.debug_info`; objdump prints it as `DW_AT_type :
  <0x2941>`.
- **EXAMPLE**: the CU for `examples/good/debug_friendly.c` contains a
  `DW_TAG_structure_type` named `point` (byte_size 8, members `x` at
  data_member_location 0 and `y` at 4), followed by `DW_TAG_subprogram` DIEs
  `main`, `manhattan`, `sum_range`, `add`, each carrying `DW_AT_low_pc` /
  `DW_AT_high_pc`. [VERIFIED]
- **COUNTEREXAMPLE**: attributes are not always present — the same
  `DW_TAG_variable` tag appears both with and without `DW_AT_location`
  (see rule 10).
- **VERIFICATION**: `objdump --dwarf=info <file>` (long form of `objdump -W`).
- **SOURCE**: `dwarf-v5` §2.8 (DIEs), §3.1.1 (compile unit), §7.5 (DIE
  representation); `binutils-docs` (objdump --dwarf).

## 3. .debug_abbrev: the schema that keeps DIEs small

- **RULE**: DIEs do not repeat attribute names. Each DIE starts with an
  "abbreviation number" that indexes `.debug_abbrev`, which declares the tag,
  the child flag, and the exact ordered list of (attribute, form) pairs.
- **WHY AI GETS IT WRONG**: agents try to parse `.debug_info` without reading
  `.debug_abbrev`; the DIE stream is meaningless without it.
- **CORRECT REASONING**: an abbreviation is like a struct layout for the DIE
  bytes. Two DIEs with the same tag but different attribute sets get different
  abbreviation numbers — that is how "this variable has no location" is
  encoded structurally.
- **EXAMPLE**: in the `-O2 -g` binary, abbrev 10 is
  `DW_TAG_variable [no children]` with only name/decl/type and NO
  `DW_AT_location`; abbrev 27 is the same tag but includes
  `DW_AT_location DW_FORM_exprloc`. `main`'s `s` and `u` use abbrev 10;
  `seed` uses a variant with `DW_AT_location`. [VERIFIED]
- **COUNTEREXAMPLE**: GCC can add per-CU abbreviations (`DW_FORM_implicit_const`)
  so abbrev numbers are not stable between files; never assume a fixed abbrev
  table.
- **VERIFICATION**: `objdump --dwarf=abbrev <file>`; cross-check the abbrev
  number printed next to each DIE in `--dwarf=info`.
- **SOURCE**: `dwarf-v5` §7.5 (abbreviations); `binutils-docs` (objdump
  --dwarf=abbrev).

## 4. Tags: what kind of thing a DIE describes

- **RULE**: the tag answers "what is this?" — `DW_TAG_compile_unit` (a
  translation unit), `DW_TAG_subprogram` (a function), `DW_TAG_inlined_subroutine`
  (an inlined call site), `DW_TAG_variable`, `DW_TAG_formal_parameter`,
  `DW_TAG_structure_type`, `DW_TAG_member`, `DW_TAG_lexical_block`,
  `DW_TAG_base_type`, `DW_TAG_pointer_type`.
- **WHY AI GETS IT WRONG**: "variables are all `DW_TAG_variable`" — function
  parameters are `DW_TAG_formal_parameter`; inlined code creates a special
  tag with `DW_AT_abstract_origin` pointing at the original subprogram.
- **CORRECT REASONING**: scope is represented by nesting: a
  `DW_TAG_lexical_block` DIE inside a subprogram wraps the variables that live
  in a compound statement, with `DW_AT_ranges`/`low_pc` for its code span.
- **EXAMPLE**: at `-O0`, `sum_range` contains a `DW_TAG_lexical_block` (low_pc
  0x1400014c4, high_pc 0x1b) whose child `i` is a `DW_TAG_variable` at
  `DW_OP_fbreg: -24`; `total` and `n` sit directly under the subprogram. [VERIFIED]
- **COUNTEREXAMPLE**: `DW_TAG_structure_type` members are `DW_TAG_member`
  children, not separate variables; a member's offset is `DW_AT_data_member_location`
  (0 and 4 for `point.x`/`point.y`), not an address. [VERIFIED]
- **VERIFICATION**: read the tag column in `objdump --dwarf=info`.
- **SOURCE**: `dwarf-v5` §2.9 (tags), §3.3 (subprograms/inlined), §3.4
  (lexical blocks), §4.1 (data objects), §5.7 (structure types).

## 5. Attributes: the facts a DIE carries

- **RULE**: attributes attach facts to a DIE: `DW_AT_name` (the source name),
  `DW_AT_type` (reference to the type DIE), `DW_AT_location` (where the value
  lives), `DW_AT_comp_dir` (working directory of the compile), `DW_AT_low_pc`
  / `DW_AT_high_pc` (code range), `DW_AT_frame_base` (how to compute the frame
  pointer), `DW_AT_decl_line`/`decl_column`/`decl_file`.
- **WHY AI GETS IT WRONG**: "DW_AT_location is a memory address"; it is a DWARF
  expression (see rule 8). "DW_AT_name is always inline text"; with DWARF v5
  names often come from the string tables via `DW_FORM_strp` /
  `DW_FORM_line_strp`.
- **CORRECT REASONING**: `DW_AT_type : <0x2941>` is a reference you must chase
  into the same CU; objdump renders the target offset. `DW_AT_frame_base :
  1 byte block: 9c (DW_OP_call_frame_cfa)` means locations using `DW_OP_fbreg`
  are offsets from the call-frame address, not from a saved rbp.
- **EXAMPLE**: the CU DIE carries `DW_AT_producer : GNU C23 16.1.0 ...
  -g -O0`, `DW_AT_language : 29 (C11)`, `DW_AT_name : (indirect line string,
  offset: 0x222): examples\good\debug_friendly.c`, `DW_AT_comp_dir : <build-dir>\
  dwarf-debug-info`, `DW_AT_stmt_list : 0x460` (offset of its line table). [VERIFIED]
- **COUNTEREXAMPLE**: `DW_AT_low_pc`/`high_pc` disappear when code is split
  into multiple ranges (common at `-O2`); the CU/subprogram then uses
  `DW_AT_ranges` instead (the `-O2 -g` CU shows `DW_AT_ranges : 0x81`). [VERIFIED]
- **VERIFICATION**: `objdump --dwarf=info` and read each attribute line.
- **SOURCE**: `dwarf-v5` §2.22 (attribute encodings), §2.7 (frame base /
  locations); `binutils-docs` (objdump --dwarf).

## 6. String tables: .debug_str and .debug_line_str (DWARF v5)

- **RULE**: to avoid repeating strings, names are stored once in
  `.debug_str` (DW_FORM_strp) and file names/comp dir in `.debug_line_str`
  (DW_FORM_line_strp). DWARF v5 added `.debug_line_str`; v4 stored line-table
  strings in `.debug_str`.
- **WHY AI GETS IT WRONG**: "the string is right after DW_AT_name" — with
  `DW_FORM_strp` the DIE holds only a 4/8-byte offset into a separate section.
- **CORRECT REASONING**: objdump resolves these automatically and prints
  `(indirect line string, offset: 0x222): examples\good\debug_friendly.c`.
- **EXAMPLE**: `.debug_line_str` (size 0x1a1d) and `.debug_str` (size 0x2f6)
  are separate sections in the `-g -O0` binary. [VERIFIED]
- **COUNTEREXAMPLE**: with `-fno-...`/v4 builds the attribute may be
  `DW_FORM_string` inline; never hard-code one layout.
- **VERIFICATION**: `objdump -h <file> | grep debug`.
- **SOURCE**: `dwarf-v5` §7.27 (string tables), §7.5.4 (forms);
  `binutils-docs` (objdump --dwarf=str / --dwarf=info).

## 7. .debug_line and the address → line mapping

- **RULE**: `.debug_line` maps machine addresses to (file, line) pairs and is
  compressed as a state-machine program, not a plain table. Each
  `DW_TAG_compile_unit` points at its line table via `DW_AT_stmt_list`.
- **WHY AI GETS IT WRONG**: agents reconstruct line numbers from disassembly
  or guess; the correct, authoritative mapping is the decoded `.debug_line`
  program.
- **CORRECT REASONING**: the line program records `(address, line, column,
  is-statement)` transitions per compilation unit; the debugger picks the
  closest entry at or before the PC.
- **EXAMPLE**: decoded with `objdump --dwarf=decodedline`, the `-g -O0`
  binary shows `debug_friendly.c 8 -> 0x140001490 (add), line 9 ->
  0x14000149e (sum = a + b)`, `line 13 -> 0x1400014b2 (sum_range)`, `line 32 ->
  0x14000151c (main)`, `line 37 -> 0x14000156c` (return). The `-O2 -g`
  binary maps several statements to the SAME address (e.g. lines 12–14 all
  start at 0x140001490), which is how the table encodes optimized-away code. [VERIFIED]
- **COUNTEREXAMPLE**: "one line == one address" is false; a line can span
  many addresses and one address can serve many lines. Ranges are per
  (file, line, view) not per instruction.
- **VERIFICATION**: `objdump --dwarf=decodedline <file> | grep <source-file>`.
- **SOURCE**: `dwarf-v5` §6.2 (line number program), §6.2.2; `binutils-docs`
  (objdump --dwarf=decodedline); `gdb-manual` (Breakpoints).

## 8. DW_AT_location and DW_OP expressions

- **RULE**: `DW_AT_location` is a DWARF expression built from `DW_OP_*`
  operations. `DW_OP_addr` pushes an absolute address (for globals);
  `DW_OP_fbreg N` is offset N from the frame base (stack locals);
  `DW_OP_regN` names a register; `DW_OP_stack_value` says the computed value is
  the location (not a pointer to it).
- **WHY AI GETS IT WRONG**: "the location is the stack offset or the register
  number" — both are only one possible encoding; optimized compilers emit
  computed expressions (e.g. `DW_OP_breg4 (rsi): -1; DW_OP_stack_value`) so the
  value is derived, not stored.
- **CORRECT REASONING**: read the byte block: `2 byte block: 91 6c` means
  `DW_OP_fbreg(-20)` (0x91 = DW_OP_fbreg, 0x6c = -20 as SLEB128).
  `DW_OP_reg0 (rax)` means the value is currently in rax.
- **EXAMPLE**: `-O0` `add`: `a` = `DW_OP_fbreg: 0`, `b` = `DW_OP_fbreg: 8`,
  `sum` = `DW_OP_fbreg: -20`; all locals live in stack slots exactly as the
  `-O0` asm shows (spills to rbp-relative slots). [VERIFIED]
- **COUNTEREXAMPLE**: `-O2` `use_reg`'s parameter `b` is
  `1 byte block: 51 (DW_OP_reg1 (rdx))` — no stack slot exists at all. [VERIFIED]
- **VERIFICATION**: `objdump --dwarf=info <file>` and read the `DW_AT_location`
  byte blocks; confirm against `objdump -d`.
- **SOURCE**: `dwarf-v5` §2.6 (location descriptions/DW_OP operations);
  `sysv-amd64-abi` §6 DWARF; `binutils-docs`.

## 9. Location lists: the value moves while the function runs

- **RULE**: when a variable's home changes (register → stack → register, or it
  is temporarily unavailable), the DIE points to a location list instead of one
  expression. DWARF v4 stored lists in `.debug_loc`; DWARF v5 renamed the
  section to `.debug_loclists` and added view/base-address encodings.
  Similarly `.debug_ranges` became `.debug_rnglists`.
- **WHY AI GETS IT WRONG**: agents assume one location per variable per
  function; at `-O2` a single variable can have several disjoint
  [begin, end) ranges with different expressions, plus gaps where it is
  genuinely unavailable.
- **CORRECT REASONING**: in the `-O2 -g` binary, `main`'s `argc` has a list:
  `0x2a10–0x2a1b in rcx`, `0x2a1b–0x2a6f in rbx`, `0x2a6f–0x2a70 at fbreg-20`,
  `0x2a70–0x2a74 in rbx`. `main`'s `argv` uses `DW_OP_entry_value` for the
  epilogue range. `sum` inside the inlined loop is reconstructed as computed
  expressions over several views. [VERIFIED]
- **COUNTEREXAMPLE**: the `-O0` binary emits NO location lists — every DIE has
  a single static exprloc. Presence of `.debug_loclists` is itself a signal
  that optimization was on.
- **VERIFICATION**: `objdump --dwarf=loc <file>` (v5: prints the
  `.debug_loclists` tables); look for `(location list)` in `--dwarf=info`.
- **SOURCE**: `dwarf-v5` §2.6 (location lists), §7.21; `gdb-manual` (Examining
  Data / optimized code); `binutils-docs` (objdump --dwarf=loc).

## 10. Why "value optimized out" happens

- **RULE**: a variable is "optimized out" when the compiler proves it has no
  home: no DW_AT_location at all (or a location list with an empty gap) for
  some PC range. This is not a bug in the debugger or a stripped binary.
- **WHY AI GETS IT WRONG**: "the debugger is broken" or "I need to recompile
  with -g" — `-g` was already given; the DIE exists but the value genuinely
  has no representation at that point, or the compiler folded the computation.
- **CORRECT REASONING**: at `-O2` GCC eliminates dead variables, keeps others
  in registers, and reconstructs some as expressions. In the bad example:
  `main`'s `s` and `u` DIEs exist but carry no `DW_AT_location` (abbrev 10);
  the inlined `use_reg` instance has `a` and `prod` without locations; the
  concrete out-of-line `use_reg` still describes `prod` (as `rcx*rdx`), because
  it is a real function with a real frame.
- **EXAMPLE**: `gdb -batch -ex "break optimized_away.c:29" -ex "run" -ex
  "print s"` → `$1 = <optimized out>`; `info locals` → `s = <optimized out>`,
  `u = <optimized out>`; backtrace shows `argv=<optimized out>`. [VERIFIED:
  gdb 17.2]
- **COUNTEREXAMPLE**: the same source built `-g -O0` prints real values:
  `r = 3`, `s = 55`, `p = {x = 3, y = -4}`, `m = 7`. [VERIFIED: gdb 17.2]
- **VERIFICATION**: `objdump --dwarf=info` — if the variable DIE has no
  `DW_AT_location`, `print` will say "optimized out"; confirm with gdb.
- **SOURCE**: `dwarf-v5` §2.6 (location presence), §3.3 (variable entries);
  `gdb-manual` (Optimized Code Debugging); `gcc-manual` (-g, -Og, -O* options).

## 11. DW_OP_entry_value: how the compiler still "remembers"

- **RULE**: `DW_OP_entry_value` describes a value as "what register/expression
  held at function entry". It lets the debugger reconstruct a parameter after
  the entry register was reused.
- **WHY AI GETS IT WRONG**: agents treat `DW_OP_entry_value` as a normal
  operation and misread the expression; it is a special "call-time snapshot"
  of a subexpression.
- **CORRECT REASONING**: GCC emits it when a parameter's home register is
  clobbered: `main`'s `argv` list ends with
  `DW_OP_entry_value: (DW_OP_reg1 (rdx)); DW_OP_stack_value`; the concrete
  `use_reg`'s `a` similarly after `imul` overwrites rcx. gdb may still print
  such values as "optimized out" if it cannot evaluate the snapshot, so an
  entry_value DIE does not guarantee a readable value.
- **EXAMPLE**: `objdump --dwarf=loc optimized_away.exe` shows several
  `DW_OP_entry_value` entries, including at offset 0x3f9 for `use_reg.a`
  (reg rcx at entry, entry_value after). [VERIFIED]
- **COUNTEREXAMPLE**: without `-g`+optimization (or with clang on some
  targets) the same parameter simply has no location — no entry_value, no value.
- **VERIFICATION**: `objdump --dwarf=loc <file>` | grep entry_value.
- **SOURCE**: `dwarf-v5` §2.6.1.2 (entry values); `gdb-manual` (Optimized Code
  Debugging); `binutils-docs`.

## 12. Inlining: abstract + concrete instances

- **RULE**: an inlined function is described by an "abstract instance" (the
  original `DW_TAG_subprogram` with `DW_AT_inline`, no locations) plus one
  `DW_TAG_inlined_subroutine` per call site, linked by `DW_AT_abstract_origin`.
  The concrete out-of-line function (if emitted) is a third DIE.
- **WHY AI GETS IT WRONG**: "there are two copies of the function in the debug
  info — the compiler is broken" or "`DW_AT_inline : 1` means always inlined".
- **CORRECT REASONING**: `DW_AT_inline : 1 (inlined)` / `: 3 (declared as
  inline and inlined)` marks the abstract instance; concrete instances repeat
  the abstract origin and add `DW_AT_low_pc`/`high_pc` and locations.
- **EXAMPLE**: `-O2 -g`: `triple` is an abstract instance
  (`DW_AT_inline : 3`); inside `main` there is a `DW_TAG_inlined_subroutine`
  with `DW_AT_abstract_origin: <0x2a03>` (use_reg) at call_line 28, and inside
  it `prod` (abstract_origin 0x2a27) has NO location. A separate concrete
  `use_reg` exists at 0x1400014e0 with full locations. [VERIFIED]
- **COUNTEREXAMPLE**: `-O0` produces no inlined_subroutine DIEs at all (static
  inline is a real call); the same source has different DIE trees by design.
- **VERIFICATION**: `objdump --dwarf=info <file>` | grep -E "inlined_subroutine|DW_AT_inline|abstract_origin".
- **SOURCE**: `dwarf-v5` §3.3 (inlined subroutine entries, abstract instances);
  `gcc-manual` (-O/-fno-inline behavior); `gdb-manual` (Optimized Code
  Debugging).

## 13. Debugging optimized code: strategies that work

- **RULE**: to get readable values: compile with `-Og` (optimize while keeping
  debug-friendly semantics) or `-O0` for correctness-focused debugging; keep
  `-g` for crash dumps and stack traces; if `-O2` is required, expect some
  "optimized out" and design for it.
- **WHY AI GETS IT WRONG**: "just add -g" or "just remove -g" are the wrong
  levers; the variable disappears because of optimization, and -g is what keeps
  the DIE around at all.
- **CORRECT REASONING**: debug-friendly code keeps values alive: avoid
  needless recomputation, keep small state as locals with real uses, don't
  rely on intermediate values that the optimizer can fold, and compile
  `-O0 -g` when stepping matters. `-g -O2` is fine for a postmortem stack
  trace but not for inspecting locals mid-function.
- **EXAMPLE**: `-O2 -g` `main` folds `s = folded_sum(seed)` and
  `u = use_reg(seed, seed+1)` into the loop; both locals are optimized out.
  Compiling the same file `-g -O0` gives gdb-printable `s`, `u`. [VERIFIED]
- **COUNTEREXAMPLE**: disabling optimization globally (`-O0`) to "fix"
  debugging changes timing, floating-point results, and concurrency behavior —
  it is a debugging aid, not a correctness fix.
- **VERIFICATION**: rebuild with `-Og -g` and re-check `info locals` in gdb.
- **SOURCE**: `gcc-manual` (Optimize Options: -Og); `gdb-manual` (Optimized
  Code Debugging); `clang-docs` (controlling debug info).

## 14. DWARF v4 vs v5: the section-name trap

- **RULE**: v5 renamed `.debug_loc` → `.debug_loclists`, `.debug_ranges` →
  `.debug_rnglists`, added `.debug_line_str`, `.debug_addr`, `.debug_str_offsets`
  and new forms (DW_FORM_implicit_const, DW_FORM_strx*). The CU header carries
  `Version: 5`.
- **WHY AI GETS IT WRONG**: documentation and older blogs describe v4; an
  agent that assumes `.debug_loc` on a v5 binary finds nothing and concludes
  "no debug info".
- **CORRECT REASONING**: check the CU `Version:` field first, then pick the
  section names and objdump switches accordingly (`--dwarf=loc` prints the
  v5 `.debug_loclists`). GCC 16.1/MinGW emits v5 by default (CU `Version: 5`,
  sections `.debug_loclists`/`.debug_rnglists`); `-gdwarf-4` forces v4.
- **EXAMPLE**: `objdump --dwarf=loc` on the `-O2 -g` binary prints
  `Contents of the .debug_loclists section: ... DWARF version: 5`. [VERIFIED]
- **COUNTEREXAMPLE**: a v4 binary's `.debug_loc` uses the older
  `DW_LLE_start_end`-style encoding; blind reuse of v5 parsing fails.
- **VERIFICATION**: `objdump --dwarf=info <file> | head`, note `Version:`;
  `objdump -h <file> | grep debug`.
- **SOURCE**: `dwarf-v5` (section layout, §7.21/§7.22); `gcc-manual`
  (-gdwarf-5 / -gdwarf-4); `binutils-docs`.

## 15. Tooling: which command shows what

- **RULE**: `objdump --dwarf=info|abbrev|decodedline|loc|ranges|str` (long) or
  `-W[iaLrR]` (short) reads DWARF; `readelf --debug-dump=...` is the ELF
  counterpart; `gdb` consumes the info interactively; `llvm-dwarfdump` is the
  LLVM-side dump tool.
- **WHY AI GETS IT WRONG**: on PE/COFF (MinGW/Windows) `readelf
  --debug-dump=info` silently produces nothing, so agents claim "no debug
  info"; `objdump -g` also prints COFF legacy info, not DWARF.
- **CORRECT REASONING**: on Windows/MinGW use `objdump --dwarf=*` (verified to
  work here). `readelf` is target verification for ELF builds. `gdb` was run
  here (gdb 17.2) and produced the "optimized out" outputs in rules 10–11.
- **EXAMPLE**: this skill was verified with `objdump --dwarf=info`,
  `--dwarf=decodedline`, `--dwarf=abbrev`, `--dwarf=loc` and gdb batch
  sessions; `readelf` and `llvm-dwarfdump` are documented as target-verification
  tools (llvm-dwarfdump not installed on this host).
- **COUNTEREXAMPLE**: don't use `nm`/`objdump -t` for line numbers or types —
  they read `.symtab`, not DWARF.
- **VERIFICATION**: `objdump --dwarf=info <file> | head -30`; then
  `gdb -batch -ex "info functions" <file>`.
- **SOURCE**: `binutils-docs` (objdump, readelf); `gdb-manual` (full manual);
  `dwarf-v5` (the format both tools parse).

## Quick lookup table

| Question | Section | Verified command |
|---|---|---|
| What DIEs exist? | .debug_info | `objdump --dwarf=info` |
| DIE encoding | .debug_abbrev | `objdump --dwarf=abbrev` |
| Address → line | .debug_line | `objdump --dwarf=decodedline` |
| Where does a var live per PC? | .debug_loclists (v5) / .debug_loc (v4) | `objdump --dwarf=loc` |
| Code ranges | .debug_rnglists (v5) / .debug_ranges (v4) | `objdump --dwarf=ranges` |
| Strings | .debug_str / .debug_line_str | `objdump --dwarf=str` |
| "Optimized out"? | DW_AT_location absent | `objdump --dwarf=info` + gdb `print` |
