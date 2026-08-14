# ELF Layout & Relocations — Rules

Each entry: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids are registry ids: `sysv-elf`, `sysv-amd64-abi`,
`binutils-docs`, `gnu-ld-manual`.

## Verification status

- VERIFIED (this host, MinGW gcc 16.1 / binutils 2.46, PE/COFF objects and executables):
  section roles (.text/.data/.bss/.rdata), symbol kinds (T/D/b/U), link failures
  (multiple definition, undefined reference), static and shared linking.
- DOCUMENTED-AS-TARGET (requires an ELF host, Linux or WSL; commands listed, NOT executed
  here): `e_ident` byte layout, `readelf -h/-S/-s/-r` output, `R_X86_64_*` relocation
  numbers and formulas, the `recompile with -fPIC` linker error.

---

## 1. ELF header: `e_ident`, class, endianness, and the identity of the file

- **RULE**: The ELF header starts with `e_ident[0..15]`: magic `0x7f 'E' 'L' 'F'`,
  `EI_CLASS` (1 = ELFCLASS32, 2 = ELFCLASS64), `EI_DATA` (1 = little-endian LSB,
  2 = big-endian MSB), `EI_VERSION` (=1). Then `e_type` (1 = ET_REL, 2 = ET_EXEC,
  3 = ET_DYN, 4 = ET_CORE), `e_machine` (EM_X86_64 = 62), `e_version`, `e_entry`
  (virtual address of entry point), and offsets/counts for the section and program
  header tables.
- **WHY AI GETS IT WRONG**: the agent reads ELF values as native little-endian x86-64
  numbers and forgets that every field is interpreted per `EI_CLASS`/`EI_DATA`; or treats
  `e_entry` as "the address of main".
- **CORRECT REASONING**: class and endianness come from the file itself; a 32-bit
  little-endian ELF is read as 32-bit LE fields. `e_entry` is a virtual address of the
  entry code (`_start` for a typical dynamically linked executable), not `main`, and the
  psABI defines the register state the loader must set up there.
- **EXAMPLE** (bad): claiming `readelf -h` output "ELF32" means the file is 32-bit code
  without checking `EI_CLASS`, or that a big-endian `e_machine` field would need byte
  swapping before comparing to `EM_X86_64`.
- **COUNTEREXAMPLE** (good): state "ELFCLASS64, little-endian (EI_DATA=1), ET_DYN for a
  PIE/shared object, EM_X86_64=62", and read all header fields in that class/endianness.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — `readelf -h file.o` shows the byte layout.
  On PE/COFF, `objdump -f` plays the same role (VERIFIED on this host).
- **SOURCE**: `sysv-elf` (ELF header, e_ident); `sysv-amd64-abi` (entry state, EM_X86_64).

## 2. Section headers vs program headers: two views of one file

- **RULE**: Section headers (`sh_*`, `readelf -S`) describe the file for the linker and
  tools: `.text`, `.data`, `.bss`, `.symtab`, `.debug_line`, each with type, flags, and
  file offsets. Program headers (`p_*`, `readelf -l`) describe how the loader maps the
  file: `PT_LOAD` segments with `p_offset`/`p_vaddr`/`p_filesz`/`p_memsz`. Sections and
  segments overlap but are not the same thing.
- **WHY AI GETS IT WRONG**: `readelf -S` and `readelf -l` outputs are conflated; an agent
  says "section `.text` is at virtual address X" from the section header, though the
  loader only reads program headers.
- **CORRECT REASONING**: the loader never walks sections; it maps `PT_LOAD` segments to
  memory at `p_vaddr`. A section belongs to a segment; `p_vaddr` for the segment, not the
  section's `sh_addr`, is what matters for runtime mapping. `SHT_NOBITS` (`.bss`) has
  `sh_size` file-space zero but `p_memsz`/memory space non-zero.
- **EXAMPLE** (bad): using `readelf -S prog` to answer "where is `.text` mapped at
  runtime" — sections give the file view.
- **COUNTEREXAMPLE** (good): `readelf -l prog`; each `PT_LOAD` shows `p_vaddr`, and on a
  PIE those addresses are relative to the load base; `.bss` appears in `p_memsz` beyond
  `p_filesz`.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — `readelf -S` and `readelf -l` on an ELF.
  PE/COFF equivalent pair (section headers + optional-header data directories) is
  VERIFIED on this host via `objdump -h`; `readelf` on PE/COFF fails with
  "Not an ELF file - it has the wrong magic bytes" (VERIFIED).
- **SOURCE**: `sysv-elf` (section header, program header); `binutils-docs` (readelf).

## 3. Common sections: .text, .data, .bss, .rodata

- **RULE**: `.text` — executable code, `SHF_ALLOC|SHF_EXECINSTR`, often read-only.
  `.data` — initialized writable data, `SHF_ALLOC|SHF_WRITE`. `.bss` — uninitialized
  data, `SHT_NOBITS`, zeroed at load. `.rodata` — constant data, `SHF_ALLOC` only,
  typically inside a read-only `PT_LOAD`. `.dynsym`/`.dynstr` — dynamic symbol table and
  its string table (only symbols the loader needs). `.got`/`.plt` — generated tables for
  position-independent access and indirect calls. `.init_array` — pointers to
  constructors run before `main`.
- **WHY AI GETS IT WRONG**: `.bss` is assumed to occupy file space; `.rodata` is assumed
  writable; `.dynsym` is conflated with `.symtab`; an agent claims the linker places code
  into these sections rather than the compiler.
- **CORRECT REASONING**: the compiler/assembler emits section directives; the linker
  collects them into segments. `SHT_NOBITS` means zero bytes in the file. Read-only
  sections land in a segment without `PF_W`; a write there faults at runtime.
- **EXAMPLE** (bad): `int x;` in `.bss`, then claiming "the object file contains a zeroed
  4-byte slot for `x`" — NOBITS: no file bytes.
- **COUNTEREXAMPLE** (good): `readelf -S` shows `.bss` with type NOBITS and `sh_size` > 0;
  `readelf -l` shows a `PT_LOAD` where `p_memsz > p_filesz` by the `.bss` size.
- **VERIFICATION**: VERIFIED on PE/COFF analog — `objdump -h sections.o` shows `.text`,
  `.data`, `.bss` (NOBITS-style size, no file content), `.rdata` (rodata equivalent).
  ELF section flags: DOCUMENTED-AS-TARGET.
- **SOURCE**: `sysv-elf` (special sections, NOBITS); `binutils-docs` (objdump -h).

## 4. Symbol table: .symtab vs .dynsym, binding and visibility

- **RULE**: `.symtab` holds all symbols for static linking and tools (`STB_LOCAL`,
  `STB_GLOBAL`, `STB_WEAK`); `strip` removes it. `.dynsym` holds only symbols the dynamic
  loader needs (exports and imports) and survives strip. `st_info` packs binding and
  type; `st_other` carries visibility (`STV_DEFAULT`, `STV_HIDDEN`, `STV_PROTECTED`,
  `STV_INTERNAL`). Only default-visibility globals in `.dynsym` are exported from a `.so`.
- **WHY AI GETS IT WRONG**: "every symbol is in `.symtab`" or "a static function is in
  `.dynsym`"; an agent reads `nm`/`readelf -s` letters without mapping them to
  binding/visibility, so hidden symbols are wrongly predicted to be exported.
- **CORRECT REASONING**: binding decides interposition (weak vs strong), visibility
  decides exportability. A `static` function is `STB_LOCAL` with `STV_HIDDEN` — never in
  `.dynsym`. `readelf -s` on a stripped `.so` lists `.dynsym` only; `nm -D` prints exactly
  the exported set.
- **EXAMPLE** (bad): predicting that a hidden-visibility global is callable from another
  `.so` at runtime — the loader cannot bind it.
- **COUNTEREXAMPLE** (good): compile with `-fvisibility=hidden` and explicit default
  exports; verify with `nm -D lib.so` that only the intended symbols remain.
- **VERIFICATION**: VERIFIED on PE/COFF analog — `nm libfoo.o` shows `T foo_add`,
  `b calls` (local static), `U` for undefined; shared build exports only the
  default-visibility globals (`objdump -p libfoo.dll` export table, size 2).
  ELF `.dynsym`/visibility semantics: DOCUMENTED-AS-TARGET.
- **SOURCE**: `sysv-elf` (symbol table, binding, visibility); `binutils-docs` (nm).

## 5. Relocation types: R_X86_64_64, R_X86_64_PC32, R_X86_64_PLT32, R_X86_64_GOTPCREL

- **RULE**: An x86-64 ELF relocation entry names a symbol `S`, an addend `A` (from the
  `r_addend` field or stored in-place), and the relocation site `P`. The x86-64 psABI
  defines the formulas:
  - `R_X86_64_64` = `S + A` (64-bit absolute, word).
  - `R_X86_64_PC32` = `S + A - P` (32-bit PC-relative, for `call`/`jmp`/addressing).
  - `R_X86_64_PLT32` = `L + A - P` where `L` is the PLT entry for `S` (linker may use it
    for external calls and relax it to PC32 when `S` is local).
  - `R_X86_64_GOTPCREL` = `G + GOT + A - P` where `G` is the offset of `S`'s GOT entry and
    `GOT` the GOT address (RIP-relative access to the GOT slot, the PIC pattern).
- **WHY AI GETS IT WRONG**: the agent treats relocations as addresses to paste, ignores
  the formula (so PC-relative vs absolute is confused), or forgets that `P` is the
  relocation site's address, not the symbol's.
- **CORRECT REASONING**: plug concrete values into the formula. `S + A - P` for a
  PC-relative reference must fit in 32 bits; an out-of-range result is a link error.
  PC-relative is position-independent; `R_X86_64_64` (absolute) is not.
- **EXAMPLE** (bad): claiming `call foo` at `0x1000` with `foo = 0x2000` yields a
  displacement of `0x2000` — correct displacement is `0x2000 - 0x1005 = 0xffb`.
- **COUNTEREXAMPLE** (good): compute `R_X86_64_PC32`: `S + A - P`, verify `readelf -r`
  shows `PLT32` for the external call and `GOTPCREL` for `&global` in PIC code.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — `readelf -r main.o` on an ELF object.
  PE/COFF analog VERIFIED — `objdump -r main.o` shows `IMAGE_REL_AMD64_REL32 foo_add`
  at offset 0x14, and the linked image shows the patched displacement in `objdump -d`.
- **SOURCE**: `sysv-amd64-abi` (relocation types and formulas); `sysv-elf` (relocation
  section structure); `binutils-docs` (readelf -r, objdump -r).

## 6. Static vs dynamic linking, and which tables survive

- **RULE**: Static linking resolves every `.symtab` reference at link time and copies code
  into the executable; the result has no `DT_NEEDED`. Dynamic linking keeps `.dynsym`
  (imports/exports), records `DT_NEEDED` dependencies, and defers resolution to load time;
  a dynamically linked executable has an interpreter (`PT_INTERP`) and the loader fixes
  up GOT/PLT relocations.
- **WHY AI GETS IT WRONG**: "shared = smaller static", so the agent expects `.symtab` in a
  shared object or expects a `.so` to be self-contained; or conflates `-l` (link-time
  search) with loader search.
- **CORRECT REASONING**: `.symtab` is stripped from shipped binaries; `.dynsym` is what
  remains. A `.so` may legally contain unresolved symbols resolved from other
  dependencies at load — the link succeeded, the loader decides.
- **EXAMPLE** (bad): `readelf -s` on a stripped executable showing only `.dynsym`, and the
  agent reporting "the program has no symbols" without noting the two-table model.
- **COUNTEREXAMPLE** (good): `readelf -s` (`.symtab`, if present) vs `readelf --dyn-syms`
  (`.dynsym`); `readelf -l` shows `INTERP` + `DYNAMIC` for a dynamically linked binary;
  `readelf -d` shows `NEEDED` libraries.
- **VERIFICATION**: VERIFIED on PE/COFF analog — static link (`gcc main.o libfoo.o`) has
  no import table; dynamic link (`gcc main.o -L. -lfoo`) records `DLL Name: libfoo.dll`
  in `objdump -p` (VERIFIED). ELF tables (`PT_INTERP`, `DT_NEEDED`): DOCUMENTED-AS-TARGET.
- **SOURCE**: `sysv-elf` (dynamic section, DT_NEEDED); `gnu-ld-manual` (linking,
  archives); `binutils-docs` (readelf -d).

## 7. Why -fPIC matters for x86-64 shared objects (R_X86_64_32S)

- **RULE**: Shared objects must be position-independent. Without `-fPIC`, code that uses
  a 32-bit absolute relocation (`R_X86_64_32S`, e.g. taking the address of a global)
  cannot be linked into a `.so`; the linker errors:
  `relocation R_X86_64_32S against symbol 'foo' can not be used when making a shared
  object; recompile with -fPIC`. `-fPIC` makes the compiler emit RIP-relative or
  GOT-based access instead.
- **WHY AI GETS IT WRONG**: "it built on Windows" (PE loader applies base relocations, so
  no PIC is needed there) or "`-fPIC` is only a performance option".
- **CORRECT REASONING**: on x86-64 ELF this is a correctness requirement, not tuning. The
  error names the *compile* flag: recompile the offending source with `-fPIC`. PE/COFF
  (MinGW) does not require `-fPIC`, so the identical source builds and runs there.
- **EXAMPLE** (bad): `int g; int *f(void){ return &g; }` built with
  `gcc -fno-pic -shared` on x86-64 ELF → `relocation R_X86_64_32S ... recompile with
  -fPIC` (DOCUMENTED-AS-TARGET).
- **COUNTEREXAMPLE** (good): same source with `gcc -fPIC -shared` → exit 0
  (DOCUMENTED-AS-TARGET). On MinGW, `gcc -fno-pic -shared` also succeeds and runs
  (VERIFIED on this host).
- **VERIFICATION**: VERIFIED on MinGW — `-fno-pic -shared` DLL builds and runs
  (exit 0), no PIC required. The ELF error text is DOCUMENTED-AS-TARGET (requires an ELF
  host to reproduce).
- **SOURCE**: `sysv-amd64-abi` (position-independent code, absolute relocations);
  `gnu-ld-manual` (shared-object linking); `sysv-elf` (text relocations).

## 8. Common agent failure: reading object-file disassembly as final code

- **RULE**: A relocation is a promise, not a value. `objdump -d` on an object file shows
  placeholder operands (`call 0`, `call 9`) plus relocation records; only after linking
  do the bytes become real addresses.
- **WHY AI GETS IT WRONG**: the agent sees `call 9 <main+9>` and reports "the call targets
  the wrong address" or "the compiler generated broken code".
- **CORRECT REASONING**: cross-check with `objdump -r`/`readelf -r`: the placeholder is
  the relocation site; the linked image contains the patched displacement.
- **EXAMPLE** (bad): declaring `main.o` broken because `objdump -d` shows a self-referential
  call placeholder.
- **COUNTEREXAMPLE** (good): `objdump -r main.o` names `foo_add` at the call's offset; the
  linked executable shows `call <foo_add>` (VERIFIED on PE/COFF analog).
- **VERIFICATION**: VERIFIED — `objdump -d main.o` placeholder `call 9 <main+9>` plus
  `objdump -r main.o` `IMAGE_REL_AMD64_REL32 foo_add`; linked `good_static.exe` shows the
  real target. ELF equivalent (`R_X86_64_PLT32 foo_add`): DOCUMENTED-AS-TARGET.
- **SOURCE**: `sysv-elf` (relocation section); `sysv-amd64-abi` (relocation formulas);
  `binutils-docs` (objdump).
