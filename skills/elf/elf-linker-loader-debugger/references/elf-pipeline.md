# The ELF Pipeline: Compiler → Linker → Loader → Debugger as One Process

A C source file does not become a running process. It becomes an object file with
relocations, then a linked image with resolved addresses, then a memory image fixed
up by the dynamic loader, and only then a sequence of instructions whose addresses a
debugger can map back to source lines. The same symbols, relocations, and ordering
rules recur at every stage; most "impossible" toolchain errors are a mismatch between
two stages.

Each entry: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE.

## Verification status

- VERIFIED (this host, MinGW gcc 16.1 / binutils 2.46, PE/COFF objects and executables,
  DWARF debug info): compilation, static and shared linking, symbol tables, relocations
  in objects and linked images, import/export tables, runtime results, gdb breakpoint and
  line mapping.
- DOCUMENTED-AS-TARGET (requires ELF host, Linux or WSL; commands listed, NOT executed
  here): readelf-based ELF inspection, ELF relocation semantics, PLT/GOT layout, dynamic
  loader resolution, `.init_array` execution order on glibc.
- Where a rule is platform-specific (PE vs ELF), both behaviors are stated and the
  verified side is marked.

---

## 1. The pipeline is one process, not four tools

- **RULE**: A symbol is created by the compiler (or assembler), referenced by relocations,
  resolved by the linker, fixed up by the loader, and queried by the debugger. Every stage
  consumes the previous stage's output and emits the next one's input.
- **WHY AI GETS IT WRONG**: tools are learned separately (`objdump` here, `gdb` there), so a
  symbol mismatch is blamed on the "compiler" even though the linker produced the error.
- **CORRECT REASONING**: decide which stage emitted the diagnostic, then walk the artifact
  backward: error text → tool → input file → symbol/relocation → source.
- **EXAMPLE** (bad): `undefined reference to 'foo_add'` at link time, and the response is to
  recompile `main.c` with different flags.
- **COUNTEREXAMPLE** (good): `nm main.o` shows `U foo_add`; `nm libfoo_wrong.o` shows
  `T foo_addition`; the real fix is making the two names agree.
- **VERIFICATION**: VERIFIED — `nm` both objects before recompiling anything.
- **SOURCE**: SysV ABI ELF (symbol table, relocation); binutils docs (nm/objdump).

## 2. ELF header, sections, and segments (two different views of one file)

- **RULE**: An ELF file has a section view (`.symtab`, `.text`, `.debug_line`; used by
  the linker and tools) and a program-header (segment) view (`PT_LOAD`, used by the loader).
  Sections describe the file; segments describe how the loader maps it into memory.
- **WHY AI GETS IT WRONG**: `readelf -h`/`-S` and `readelf -l` outputs are conflated, and
  `p_offset` vs `p_vaddr` (file offset vs virtual address) are treated as identical.
- **CORRECT REASONING**: the loader only reads program headers; `p_vaddr` for a `PT_LOAD`
  segment is where it must land in the address space. `e_entry` (entry point) is a virtual
  address in a loaded segment.
- **EXAMPLE** (bad): claiming `readelf -S prog | grep .text` tells you where `.text` is
  mapped at runtime.
- **COUNTEREXAMPLE** (good): `readelf -l prog` and note each `PT_LOAD` `p_vaddr`/`p_offset`;
  on a PIE the `p_vaddr`s are relative to the load base.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — `readelf -h`, `readelf -S`, `readelf -l` on a
  Linux ELF. On PE/COFF the equivalent pair is section headers and the data directories in
  the optional header (VERIFIED: `objdump -h` works, `readelf` rejects PE with
  "Not an ELF file - it has the wrong magic bytes").
- **SOURCE**: SysV ABI ELF (ELF header, sections, program headers); binutils docs (readelf).

## 3. Symbol table: what the linker can see

- **RULE**: A non-static function or object has external linkage and appears in the symbol
  table as a global (`T` text / `D` data); `static` and file-local symbols are local (`t`/`d`/`b`).
  Undefined references appear as `U`. Only default-visibility globals are exported from a
  shared object into `.dynsym`.
- **WHY AI GETS IT WRONG**: "the function is in the file, so the linker sees it" — but a
  `static` function, a hidden-visibility function, or a symbol never referenced by any
  object is invisible or discarded.
- **CORRECT REASONING**: `nm` letters are the ground truth: `T`/`D` defined global,
  `U` undefined, `W` weak, `t`/`d`/`b` local. The linker resolves every `U` against some
  `T`/`D` in the input objects, archives, or shared libraries.
- **EXAMPLE** (bad): a library exposes `int helper(void)` internally, but the executable
  references `int helper(int)` — `nm` shows `T helper` and `U helper` with no match at link.
- **COUNTEREXAMPLE** (good): the header declares exactly what the library defines, and
  `nm` shows the `U` in the caller and a matching `T` in the library.
- **VERIFICATION**: VERIFIED — `nm libfoo.o` shows `T foo_add` and `b calls`; `nm main.o`
  shows `U foo_add`. Shared build exports only `foo_add`/`foo_get_calls` (VERIFIED via
  `objdump -p libfoo.dll` export table).
- **SOURCE**: SysV ABI ELF (symbol table, binding, visibility); binutils docs (nm).

## 4. Relocations: the compiler leaves placeholders for the linker

- **RULE**: every reference to an external symbol compiles to an instruction with a
  placeholder operand plus a relocation record that tells the linker which bytes to patch
  and how to compute the value. On x86-64 ELF, `R_X86_64_PC32`/`PLT32` computes
  `S + A - P` (symbol value + addend - position).
- **WHY AI GETS IT WRONG**: disassembling an object file shows `call 0` / `call 9` and the
  agent concludes the code is broken, or assumes the bytes are final.
- **CORRECT REASONING**: an object is pre-link; `e8 00 00 00 00` means "call with a
  relocation at this offset". The linker fills the relative displacement. In the final
  executable the same call shows a real target.
- **EXAMPLE** (bad): inspecting `objdump -d main.o`, seeing `call 9 <main+9>`, and reporting
  "the call goes to the wrong address".
- **COUNTEREXAMPLE** (good): run `objdump -r main.o`; the `IMAGE_REL_AMD64_REL32` record at
  offset 0x14 names `foo_add`; after linking, `objdump -d` shows `call 140001490 <foo_add>`.
- **VERIFICATION**: VERIFIED on PE/COFF — `objdump -r main.o` lists
  `IMAGE_REL_AMD64_REL32 foo_add` at `.text.startup+0x14`, and the linked `good_static.exe`
  contains `call 140001490 <foo_add>`. ELF equivalent: DOCUMENTED-AS-TARGET —
  `readelf -r main.o` shows `R_X86_64_PLT32 foo_add` and `objdump -d` after link.
- **SOURCE**: x86-64 psABI (relocation types and `S + A - P`); SysV ABI ELF (relocation
  section); binutils docs (objdump).

## 5. Static linking: the linker must resolve every `U`

- **RULE**: when linking an executable from objects, every undefined symbol must be defined
  by an object, archive, or linked-in library. Two strong definitions of the same symbol
  are a `multiple definition` error. Weak symbols (`__attribute__((weak))`) are the
  documented exception.
- **WHY AI GETS IT WRONG**: "it compiled, the error is a fluke" — or the agent edits source
  instead of inspecting which objects were on the command line.
- **CORRECT REASONING**: `undefined reference` = a `U` symbol with no `T`/`D` provider on
  the link line; `multiple definition` = two providers. Archive search order matters:
  `ld` only pulls archive members that resolve a currently-undefined symbol.
- **EXAMPLE** (bad): linking `main.o libfoo_wrong.o` where `main.c` calls `foo_add` but
  `libfoo_wrong.c` defines `foo_addition` → `undefined reference to 'foo_add'` (VERIFIED).
- **COUNTEREXAMPLE** (good): `gcc main.o libfoo.o` where `nm` shows matching `U`/`T`;
  VERIFIED — exit 0, program prints `foo_add(2, 3) = 5`.
- **VERIFICATION**: VERIFIED — both the mismatch failure (link exit 1, ld diagnostics) and
  the clean static link (exit 0) were reproduced. DOCUMENTED-AS-TARGET — same commands on
  ELF: `gcc main.o libfoo.o -o prog`.
- **SOURCE**: SysV ABI ELF (symbol resolution); GNU ld manual (archive search, weak symbols).

## 6. Multiple definition is a linker error, not a warning

- **RULE**: two objects both defining `foo_add` produce `multiple definition of 'foo_add';
  first defined here`. A tentative definition in C (`int x;` at file scope) merges under
  common rules; a strong initialized definition does not.
- **WHY AI GETS IT WRONG**: "the second definition just overrides the first" — true for
  some languages, false for the C linker model with strong symbols.
- **CORRECT REASONING**: each strong definition must be unique in the link. Duplicates
  usually mean two TUs both define the symbol, or the same object is on the line twice.
- **EXAMPLE** (bad): `a.c` defines `foo_add`, `b.c` defines `foo_add` again and links
  `gcc a.c b.c` → `multiple definition of 'foo_add'` (VERIFIED, exit 1).
- **COUNTEREXAMPLE** (good): one definition plus one `extern` declaration; the linker sees
  one `T` and one `U`.
- **VERIFICATION**: VERIFIED on PE/COFF — exact ld diagnostics captured. ELF `ld` reports
  the same two-line message; DOCUMENTED-AS-TARGET.
- **SOURCE**: SysV ABI ELF (symbol binding rules); GNU ld manual.

## 7. Static vs dynamic linking, and what "shared" actually changes

- **RULE**: static linking copies code and data into the executable at link time. Dynamic
  linking records `DT_NEEDED` dependencies and defers symbol resolution to load time.
  The dynamic linker (on Linux `ld.so`) maps each dependency's `PT_LOAD` segments, applies
  relocations, and runs initializers in dependency order.
- **WHY AI GETS IT WRONG**: "shared = the same as static but smaller" — shared changes the
  error surface (load-time vs link-time), the export surface (`.dynsym` vs `.symtab`), and
  the linking model (interposition).
- **CORRECT REASONING**: on ELF a `.so` may legitimately contain unresolved symbols; they
  are resolved by the executable or other dependencies at load. PE differs: the MinGW
  toolchain refuses an undefined symbol inside a `-shared` DLL at link time (VERIFIED
  below), while ELF defers it.
- **EXAMPLE** (bad): a DLL built with `gcc -shared` containing a call to a missing
  `qux_missing` — MinGW link fails immediately: `undefined reference to 'qux_missing'`
  (VERIFIED, exit 1, even with `-Wl,--allow-shlib-undefined`).
- **COUNTEREXAMPLE** (good): keep shared-library code free of undefined symbols, or make
  them weak if the executable may provide them.
- **VERIFICATION**: VERIFIED — the DLL link above exits 1. DOCUMENTED-AS-TARGET — on ELF
  the same source builds with `gcc -shared -o libbar.so libbar.c` (undefined symbol kept),
  the executable links, and the loader reports the error at runtime: for data symbols at
  load, for functions at first call under lazy binding; `LD_DEBUG=symbols ./prog` shows it.
- **SOURCE**: SysV ABI ELF (dynamic section, DT_NEEDED); GNU ld manual (`--no-undefined`,
  `-z defs`); x86-64 psABI (dynamic linking).

## 8. `-fPIC` and text relocations: a correctness rule on x86-64 ELF

- **RULE**: shared objects must be position-independent. Non-PIC code can contain 32-bit
  absolute relocations (e.g. `R_X86_64_32S` for taking the address of a global); the
  linker rejects them when making a shared object. `-fPIC` forces RIP-relative addressing.
- **WHY AI GETS IT WRONG**: "it built on Windows" or "`-fPIC` is only for performance".
  On ELF x86-64 it is a hard requirement; on PE it is not needed at all because the loader
  applies base relocations to the whole image.
- **CORRECT REASONING**: compile shared-library sources with `-fPIC`. If a `.so` link fails
  with `relocation R_X86_64_32S ... recompile with -fPIC`, the fix is the compile flag, not
  a linker flag. On MinGW/PE the identical source builds and runs without `-fPIC`
  (VERIFIED: `-fno-pic -shared` exit 0, binary runs, exit 0).
- **EXAMPLE** (bad): `libfoo_nopic.c` defines `int foo_global; int *foo_addr(void) { return
  &foo_global; }` and is linked with `gcc -fno-pic -shared -o libfoo.so` — on ELF this
  errors: `relocation R_X86_64_32S against symbol 'foo_global' can not be used when making
  a shared object; recompile with -fPIC` (DOCUMENTED-AS-TARGET).
- **COUNTEREXAMPLE** (good): same file compiled `gcc -fPIC -shared -o libfoo.so`
  (DOCUMENTED-AS-TARGET). On MinGW the `-fno-pic` variant works (VERIFIED).
- **VERIFICATION**: VERIFIED on MinGW — `-fno-pic -shared` DLL build exit 0, run exit 0;
  RIP-relative `addl $1, calls(%rip)` visible in `objdump -s`/`-d`. ELF failure mode
  DOCUMENTED-AS-TARGET (needs an ELF host).
- **SOURCE**: x86-64 psABI (relocation types, position independence); GNU ld manual;
  SysV ABI ELF (text relocations).

## 9. PLT/GOT and lazy binding: the first call is special

- **RULE**: calls to external functions go through the Procedure Linkage Table. The PLT
  entry jumps to a Global Offset Table slot; on the first call the slot points to the
  dynamic linker's resolver, which resolves the symbol, patches the GOT slot, and jumps to
  the target. Later calls go straight to the function. `-z now` (or `LD_BIND_NOW`) forces
  all bindings at load.
- **WHY AI GETS IT WRONG**: "every external call goes through the dynamic linker" — only
  the first does under lazy binding; and agents forget the failure timing consequence
  (an undefined function's error appears on first call, not at load).
- **CORRECT REASONING**: distinguish the GOT entry (data, relocated eagerly where needed)
  from the PLT stub (code). `readelf -r` shows `R_X86_64_JUMP_SLOT` entries; `objdump -R`
  shows the corresponding PE import table. Resolution timing is observable with
  `LD_DEBUG=bindings`.
- **EXAMPLE** (bad): predicting that an undefined *function* in a `.so` fails the executable
  at startup under the default lazy binding — on ELF it fails at first call, not at load.
- **COUNTEREXAMPLE** (good): predict load-time failure for an undefined *data* symbol
  (GOT relocation applied eagerly) and first-call failure for functions; disable lazy
  binding with `LD_BIND_NOW`/`-z now` to make both fail at load.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — `objdump -R prog` (JUMP_SLOT relocations),
  `gdb` on a `.so` function with lazy binding, `LD_DEBUG=bindings ./prog`. On PE/COFF the
  import table is the analogous artifact and is VERIFIED: `objdump -p good_dyn.exe` shows
  `DLL Name: libfoo.dll` with imports `foo_add`, `foo_get_calls`.
- **SOURCE**: SysV ABI ELF (dynamic linking); x86-64 psABI (PLT/GOT sections); GNU ld
  manual (`-z now`); binutils docs (objdump `-R`).

## 10. `.init_array`, `.ctors`, and the path from `_start` to `main`

- **RULE**: the ELF entry point `_start` is not `main`. The x86-64 psABI defines the entry
  state (stack holds argc, argv, envp, auxiliary vector; `%rdx` points to a function to
  register with `atexit`). On glibc, `_start` calls `__libc_start_main`, which runs
  `.preinit_array` then `.init_array` functions in order, then calls `main`. `.fini_array`
  runs at exit in reverse order. `DT_INIT_ARRAY`/`DT_FINI_ARRAY` semantics come from the
  SysV ABI.
- **WHY AI GETS IT WRONG**: "constructors run inside `main`" or "`main` is the first user
  code" — `.init_array` runs before `main`, and ordering across a dependency chain is the
  loader's choice, so initialization order is only guaranteed within one object.
- **CORRECT REASONING**: `_start` (linker-provided/crt) → `__libc_start_main` → preinit →
  init array → `main` → `exit` → fini array → `_fini`. Tools: `readelf -d` shows
  `INIT_ARRAY`/`FINI_ARRAY` pointers; `nm`/`objdump` show `_start` in `crt1.o`/`crt0.o`.
- **EXAMPLE** (bad): placing an `__attribute__((constructor))` function in `main.c` and
  asserting it runs "inside main before anything else" — it runs before `main`, but in
  an order relative to other init functions that depends on link/load order.
- **COUNTEREXAMPLE** (good): rely only on per-object ordering guarantees; for cross-object
  ordering use explicit dependency resolution rather than constructor ordering.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — `readelf -d prog | grep -i init`, break at
  `_start` vs `main` in gdb, and `gdb -batch -ex "break main" -ex run` to observe startup
  work. VERIFIED partial: `nm` on linked MinGW binaries shows `__main` (the PE CRT
  constructor runner) being called from `main`'s prologue (`call <__main>` in disassembly).
- **SOURCE**: x86-64 psABI (3.4 Starting an Application; `.init_array`/`.fini_array`);
  SysV ABI ELF (DT_INIT_ARRAY semantics); GDB manual (breakpoints).

## 11. The dynamic loader resolves what the linker left open

- **RULE**: at load, `ld.so` maps segments, processes relocations, resolves
  `DT_NEEDED` dependencies, and binds symbols in a defined search order: the executable
  first, then its dependencies. Where libraries are found is controlled by `RPATH`/
  `RUNPATH`, `LD_LIBRARY_PATH`, and the default cache/config; `LD_PRELOAD` injects
  libraries before everything.
- **WHY AI GETS IT WRONG**: link and load are conflated — `-L` affects the link-time search
  for files, but the runtime search is a different mechanism. "Cannot find -lfoo" (link)
  vs "cannot open shared object file" (load) are different errors.
- **CORRECT REASONING**: link-time: `-L` directories + `-l` names. Load-time: rpath/runpath
  (embedded at build with `-Wl,-rpath`), `LD_LIBRARY_PATH` (env, overridden by setuid),
  loader config. Diagnose by which tool emitted the message.
- **EXAMPLE** (bad): "I passed `-L/tmp/mylibs` and it still can't open the library" —
  `-L` only affects linking; without `-Wl,-rpath,/tmp/mylibs` or `LD_LIBRARY_PATH`, the
  loader at runtime doesn't know the location.
- **COUNTEREXAMPLE** (good): link with `-Wl,-rpath,'$ORIGIN'` for loaders relative to the
  binary, or set `LD_LIBRARY_PATH`; verify with `ldd`/`objdump -p`.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — `LD_DEBUG=files LD_LIBRARY_PATH=. ./prog`,
  `ldd ./prog`. VERIFIED on PE — `objdump -p good_dyn.exe` shows the loader must find
  `libfoo.dll`; removing it changes a link-time-correct binary into a load-time failure
  (the DLL's absence is reported by the OS loader, not the linker).
- **SOURCE**: SysV ABI ELF (dynamic section, DT_NEEDED); GNU ld manual (`-rpath`, `-rpath-link`, `--as-needed`).

## 12. The debugger maps addresses to source via DWARF

- **RULE**: gdb resolves `break main`, `info line`, and backtraces using DWARF: the line
  program in `.debug_line` maps addresses to source lines, `.debug_info` carries function
  scopes, types, and variable location descriptions (location lists, `DW_OP` expressions).
  Without DWARF (stripped binary) gdb falls back to the symbol table and can only show
  function names, not lines or variables.
- **WHY AI GETS IT WRONG**: "the debugger reads the source file" — the debugger reads the
  debug info that the compiler wrote; the source files are only opened to display lines.
  And "if it compiled with -g it will show everything" — optimization still removes or
  relocates variables.
- **CORRECT REASONING**: `-g` emits DWARF; `-O2` lets the compiler drop or rematerialize
  values, so gdb shows `a=a@entry=2` (call-site entry value) or "optimized out". Break at
  the function, not at the line, if the prologue was optimized away.
- **EXAMPLE** (bad): on an `-O2` binary, expecting a full local-variable view inside a
  function whose locals were kept in registers only briefly — gdb reports them optimized out.
- **COUNTEREXAMPLE** (good): rebuild with `-O0 -g` for full variable access, or rely on
  entry values; verify with `info args`, `info locals`, `info line`.
- **VERIFICATION**: VERIFIED on MinGW (DWARF in PE) — `gdb -batch -ex "info line main" -ex
  "break main" -ex run -ex step -ex bt good_g0.exe` maps line 5 of `main.c` to address
  `0x140001490`, breaks at `main.c:6`, steps into `foo_add` at `libfoo.c:7`, and prints both
  frames; at `-O2` gdb prints `foo_add (a=a@entry=2, b=b@entry=3)` and `No locals.` for the
  static `calls`. DOCUMENTED-AS-TARGET — identical commands on ELF, plus
  `readelf --debug-dump=decodedline` to read `.debug_line` directly.
- **SOURCE**: DWARF v5 (.debug_line, .debug_info, location lists); GDB manual (breakpoints,
  Optimized Code Debugging); binutils docs (readelf --debug-dump).
