# Dynamic Linking: GOT/PLT & Lazy Binding — Rules

Each entry: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids are registry ids: `sysv-elf`, `sysv-amd64-abi`,
`binutils-docs`, `gdb-manual`.

## Verification status

- VERIFIED (this host, MinGW gcc 16.1 / binutils 2.46, PE/COFF): DLL build with exported
  API, executable importing it, export table contents, import thunk stubs (`jmp
  *rel32(%rip)` through `__imp_*` IAT slots), indirect calls through the IAT,
  `objdump -p` import/export output, `objdump -R` failure on PE ("not a dynamic
  object"), the file-local-symbol link failure, and the absence of PE interposition.
- DOCUMENTED-AS-TARGET (requires an ELF host, Linux or WSL; commands listed, NOT
  executed here): ELF GOT/PLT layout, lazy binding via `_dl_runtime_resolve`, eager
  binding via `LD_BIND_NOW`/`-z now`, `readelf -d` dynamic entries, `objdump -R`
  `JUMP_SLOT`/`GLOB_DAT`/`RELATIVE`, `-fPIC`/`-fPIE` codegen, symbol interposition.
- The PE/COFF analog is structural (import thunk ≈ PLT stub, IAT slot ≈ GOT slot) but
  the Windows loader resolves imports eagerly, so PE shows NO lazy-binding analog and NO
  symbol interposition by default.

---

## 1. GOT and PLT are the two tables behind every cross-boundary reference

- **RULE**: In a dynamically linked x86-64 ELF program, an external *function* is called
  through the PLT (Procedure Linkage Table); external *data* is reached through the GOT
  (Global Offset Table). The GOT holds resolved addresses; the PLT holds small stubs
  that indirect through GOT slots. Sections: `.got` (data addresses, `DT_RELA`
  relocations), `.got.plt` (function addresses, `DT_JMPREL` relocations), `.plt`
  (stubs). The first three `.got.plt` entries are reserved for the dynamic linker
  (`link_map`, resolver address, and the resolver entry `PLT0` uses them).
- **WHY AI GETS IT WRONG**: the agent treats "external call" as one mechanism and
  "external data access" as another; or claims the GOT already contains final addresses
  at load with no relocations involved.
- **CORRECT REASONING**: separate by symbol kind — functions go through PLT stubs, data
  through GOT slots. Both tables are filled by the dynamic loader from the relocation
  tables, not by the linker. Which table a reference uses is visible in the object file
  as `R_X86_64_PLT32` (call) vs `R_X86_64_GOTPCREL` (data address) relocations.
- **EXAMPLE** (bad): "the call jumps straight to the library code because the linker
  patched the address".
- **COUNTEREXAMPLE** (good): `objdump -R prog` shows `R_X86_64_JUMP_SLOT` relocations
  for every external function and `R_X86_64_GLOB_DAT` for every external data symbol;
  the PLT stub for symbol `n` is `jmp *GOT.plt[n]`.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — `objdump -R` and `objdump -d` on an ELF
  binary. PE analog VERIFIED on this host: `objdump -d` shows
  `jmp *0x6e0a(%rip)  # 1400082a0 <__imp_counter_reset>` — the import thunk (PLT analog)
  indirects through the IAT slot (GOT analog); `objdump -p` lists the imports.
- **SOURCE**: `sysv-elf` (special sections, dynamic section); `sysv-amd64-abi`
  (position-independent code); `binutils-docs` (objdump -R, -p).

## 2. Lazy binding: first call → PLT stub → resolver → GOT patched

- **RULE**: With the default lazy binding, an external function's GOT slot is not
  resolved at load. The first call enters the PLT stub, which jumps through the GOT
  slot; the slot still points at the "push reloc index / jump to PLT0" continuation.
  PLT0 calls the resolver (`_dl_runtime_resolve` in glibc), which looks up the symbol
  and writes the resolved address into the GOT slot. Every later call jumps directly.
- **WHY AI GETS IT WRONG**: "PLT entries contain final addresses", "lazy binding only
  affects startup speed", or "an undefined function fails at load like an undefined
  data symbol does".
- **CORRECT REASONING**: a lazy PLT entry is a thunk whose GOT slot is initially the
  resolver path and is patched exactly once, on the first call. Consequence for failure
  timing: an undefined *function* fails at the first call; an undefined *data* symbol
  fails at load (data has no lazy binding).
- **EXAMPLE** (bad): explaining "the program crashed on startup" for a missing function
  that is actually only reached mid-run.
- **COUNTEREXAMPLE** (good): run with `LD_DEBUG=bindings prog`; the first call to
  `foo` produces a binding line, and `gdb` stepping the first call passes through the
  resolver while subsequent calls do not.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — `LD_DEBUG=bindings`, gdb breakpoint on the
  first-call path, or `objdump -R` showing `R_X86_64_JUMP_SLOT` entries.
- **SOURCE**: `sysv-elf` (dynamic linking, JUMP_SLOT); `gdb-manual` (observing the
  resolver call in the debugger).

## 3. Eager binding: `LD_BIND_NOW`, `-z now`, `DT_BIND_NOW`

- **RULE**: `LD_BIND_NOW=1 prog` (environment, at run time) or `-z now` (linker flag, at
  link time) forces all dynamic relocations to be resolved at load. The loader then
  fails at startup if any symbol is undefined, and the GOT is fully populated before
  `main`. `readelf -d` shows `BIND_NOW` / `FLAGS` with the NOW bit (and `FLAGS_1` NOW)
  when `-z now` was used.
- **WHY AI GETS IT WRONG**: `LD_BIND_NOW` is described as "a startup optimization" or
  its *observable consequence* is missed — it changes *when* resolution failures occur,
  not whether the program links.
- **CORRECT REASONING**: eager binding trades load-time cost for immediate detection and
  for hardened RELRO (GOT becomes read-only after relocation). With lazy binding a
  missing function is discovered on first use; with eager binding the same program dies
  at load.
- **EXAMPLE** (bad): "LD_BIND_NOW makes the program faster, so use it by default".
- **COUNTEREXAMPLE** (good): an executable with a function that exists in no `DT_NEEDED`
  library runs to the call site under default binding but fails at startup under
  `LD_BIND_NOW=1`; that difference is the correct way to explain the variable.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — `LD_BIND_NOW=1 ./prog` vs `./prog` on an ELF
  host with a deliberately missing function; `readelf -d` with/without `-z now`.
- **SOURCE**: `sysv-elf` (dynamic section, DT_BIND_NOW, FLAGS); `binutils-docs`
  (readelf -d); `gnu-ld-manual` is used by the sibling skill for the `-z` option set.

## 4. `.got` vs `.got.plt` and the `DT_*` pointers that locate them

- **RULE**: `.got` holds data-symbol addresses and other absolute pointers
  (`DT_RELA` relocations, `R_X86_64_GLOB_DAT` / `R_X86_64_RELATIVE`); `.got.plt` holds
  function-address slots with the three reserved entries at the start
  (`DT_PLTGOT` points at the start of `.got.plt`; `DT_JMPREL` points at the
  `R_X86_64_JUMP_SLOT` relocation array for the PLT). `DT_RELASZ`/`DT_RELAENT` describe
  the `.rela.dyn` array.
- **WHY AI GETS IT WRONG**: GOT and `.got.plt` are conflated; the reserved entries are
  mistaken for real data slots; `DT_PLTGOT` vs `DT_JMPREL` are swapped.
- **CORRECT REASONING**: `DT_JMPREL` entries are exactly the lazily-bound PLT/GOT
  relocations; `DT_RELA` entries are the eagerly-bound data relocations. `readelf -d`
  names both arrays; their sizes (`PLTRELSZ`, `RELASZ`) and entry sizes
  (`PLTREL`, `RELAENT`) let you verify the counts match `objdump -R`.
- **EXAMPLE** (bad): claiming GOT[0] "holds the address of the first global variable".
- **COUNTEREXAMPLE** (good): `readelf -d` on a PIE shows `PLTGOT`, `PLTRELSZ`,
  `JMPREL`, `RELA`; the count of `JUMP_SLOT` relocations equals the number of PLT
  entries minus the reserved GOT slots.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — `readelf -d`, `readelf -S`, `objdump -R`.
- **SOURCE**: `sysv-elf` (dynamic section tags); `binutils-docs` (readelf).

## 5. Relocation formulas that drive the tables: `R_X86_64_PLT32` and `R_X86_64_GOTPCREL`

- **RULE**: On x86-64, a call to an external function is `R_X86_64_PLT32`:
  `L + A - P`, where `L` is the PLT entry for the symbol, `A` the addend, `P` the
  relocation site (linker relaxes it to `R_X86_64_PC32` = `S + A - P` when the symbol
  is not preemptible). An address of (or access to) external data is
  `R_X86_64_GOTPCREL`: `G + GOT + A - P`, where `G` is the offset of the symbol's GOT
  entry and `GOT` the GOT base — i.e. RIP-relative addressing of the GOT slot, then an
  indirect load. `R_X86_64_64` (`S + A`, absolute) is not position-independent and is
  forbidden in a normal `.so`.
- **WHY AI GETS IT WRONG**: PLT32 is read as "the target is the PLT" (it is: `L`), but
  GOTPCREL is misread as "the target is the symbol" — it is a *slot address*; or the
  agent forgets the `-P` term so out-of-range and preemption arguments come out wrong.
- **CORRECT REASONING**: plug into the formula. GOTPCREL yields an address *of the GOT
  entry*, and the instruction loads the symbol's value from there — one extra memory
  hop, which is why non-preemptible references get relaxed to direct PC-relative.
- **EXAMPLE** (bad): "`call foo` compiles to `call <foo>` with an absolute address in
  PIC code".
- **COUNTEREXAMPLE** (good): `readelf -r lib.o` shows `R_X86_64_PLT32 foo` for the
  external call and `R_X86_64_GOTPCREL` for `&external_global`; `readelf -r` on the
  linked `.so` shows the JUMP_SLOT/GLOB_DAT runtime form of the same references.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — `readelf -r` on object and linked image.
  PE analog VERIFIED: `objdump -r` shows `IMAGE_REL_AMD64_REL32` on the object; the
  linked exe shows indirect `call *rel32(%rip) # __imp_*`.
- **SOURCE**: `sysv-amd64-abi` (relocation types and formulas); `sysv-elf` (relocation
  sections); `binutils-docs` (readelf -r, objdump -r).

## 6. `readelf -d`: what a DYNAMIC section really says

- **RULE**: `readelf -d prog` prints the dynamic section: `NEEDED` (each library the
  loader must load, in order), `SONAME`, `PLTGOT`, `PLTRELSZ`, `PLTREL`, `JMPREL`,
  `RELA`/`RELASZ`/`RELAENT`, `INIT`/`FINI`/`INIT_ARRAY`, `FLAGS` and `FLAGS_1`
  (NOW when eager binding is forced), `BIND_NOW` (legacy tag). `NEEDED` names do NOT
  include paths — the loader searches per `DT_RPATH`/`DT_RUNPATH`/env (`LD_LIBRARY_PATH`)
  and defaults.
- **WHY AI GETS IT WRONG**: the agent reads `NEEDED` as "files that were linked" (they
  are load-time dependencies), or claims `BIND_NOW` vs lazy binding from `FLAGS` without
  checking `FLAGS_1`/`JMPREL`.
- **CORRECT REASONING**: the dynamic section is a contract between linker and loader:
  which libraries, which relocation arrays, which pointers, and whether to bind eagerly.
  Presence of `JMPREL` + absence of NOW ⇒ lazy binding by default.
- **EXAMPLE** (bad): "`readelf -d` shows `NEEDED libfoo.so`, so libfoo is statically
  linked".
- **COUNTEREXAMPLE** (good): `readelf -d` shows `NEEDED` libraries that still exist as
  `.so` files, `JMPREL` with `PLTRELSZ` > 0 for lazy binding, and `FLAGS_1: NOW` when
  built with `-z now`.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — `readelf -d` on ELF. PE analog VERIFIED:
  `objdump -p` lists `DLL Name: libcounter.dll` in the exe's import table.
- **SOURCE**: `sysv-elf` (dynamic section); `binutils-docs` (readelf -d, objdump -p).

## 7. `objdump -R`: the loader's worklist

- **RULE**: `objdump -R prog` prints the *dynamic* relocations the loader must apply,
  not object relocations: `R_X86_64_RELATIVE` (base-address-relative pointers, in a
  PIE/`.so`), `R_X86_64_GLOB_DAT` (data GOT slots), `R_X86_64_JUMP_SLOT` (PLT/GOT
  function slots, the lazy-binding list). Counts here must match `readelf -d`
  (`RELASZ` vs `JMPREL` sizes).
- **WHY AI GETS IT WRONG**: `objdump -R` is run on an object file (it prints nothing
  useful there — the object needs `objdump -r`), or JUMP_SLOT is described as "already
  resolved".
- **CORRECT REASONING**: `-r` = relocations *in this object* (linker input); `-R` =
  dynamic relocations *in the final image* (loader input). JUMP_SLOT entries are the
  exact set of lazy PLT slots; under `LD_BIND_NOW` they are applied at load and the
  resolved addresses land in `.got.plt`.
- **EXAMPLE** (bad): running `objdump -R main.o` and reporting "no relocations, the
  object is already linked".
- **COUNTEREXAMPLE** (good): `objdump -R prog` lists `R_X86_64_JUMP_SLOT` for each
  external call and `R_X86_64_GLOB_DAT`/`R_X86_64_RELATIVE` for data; verify the
  JUMP_SLOT count against `readelf -d` `PLTRELSZ / 24`.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — `objdump -R` on an ELF binary.
  VERIFIED on this host: `objdump -R` on the PE exe errors
  `not a dynamic object` — PE imports are listed by `objdump -p` instead.
- **SOURCE**: `binutils-docs` (objdump); `sysv-elf` (dynamic relocations).

## 8. Symbol interposition and the lookup scope

- **RULE**: The loader resolves an undefined symbol against the global scope: the main
  program first, then each `DT_NEEDED` library breadth-first. An executable's symbol
  participates in this scope only if it is in the executable's dynamic symbol table
  (`.dynsym`) — that requires `-rdynamic`/`--export-dynamic` (or being a needed export)
  because executable symbols are not exported by default. A default-visibility global in
  a `.so` is preemptible: the library's own internal calls to it may bind to another
  definition found earlier in the scope. `-Bsymbolic`/`-Bsymbolic-functions` force
  local binding; `STV_PROTECTED` prevents preemption of that symbol.
- **WHY AI GETS IT WRONG**: "interposition always happens", "a `.so`'s internal calls
  always stay inside the `.so`", or "the executable's symbols always win".
- **CORRECT REASONING**: interposition is a loader decision based on scope order and on
  what is in `.dynsym`. Default executable: not interposing. `-rdynamic` executable:
  its globals preempt same-named library symbols. Check `nm -D`/`readelf --dyn-syms`
  to see what is actually in the scope.
- **EXAMPLE** (bad): expecting the library's internal `helper` call to hit the
  executable's `helper` without `-rdynamic`.
- **COUNTEREXAMPLE** (good): link the executable with `-rdynamic` and observe the
  library's internal call now resolving to the executable's definition
  (`LD_DEBUG=bindings` shows the binding), then remove `-rdynamic` and observe the
  library's own definition winning again.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — `LD_DEBUG=bindings`, `nm -D`, `readelf
  --dyn-syms`. PE contrast VERIFIED: the same duplicate-symbol program linked against a
  DLL runs with the DLL's internal calls going to the DLL's own copy
  (`compute(1) = 3`, exit 0) — no interposition on PE; the same two objects statically
  linked fail with `multiple definition of 'helper'` (exit 1).
- **SOURCE**: `sysv-elf` (symbol binding, visibility, dynamic symbol table);
  `sysv-amd64-abi` (preemption); `binutils-docs` (nm -D, readelf --dyn-syms).

## 9. `-fPIC` vs `-fPIE`: who is expected to move, and what code they emit

- **RULE**: `-fPIC` is for code that will be a shared object (ET_DYN, loadable at any
  base, and possibly *preempted*): external functions go through the PLT
  (`R_X86_64_PLT32`) and external data through the GOT (`R_X86_64_GOTPCREL`).
  `-fPIE` is for an executable that will be linked `-pie` (also ET_DYN): the compiler
  may assume its own symbols are not preemptible, so calls within the executable relax
  to direct `R_X86_64_PC32` while references to undefined (imported) symbols still use
  PLT/GOT. Without either, a 32-bit absolute relocation appears, which cannot link into
  a shared object.
- **WHY AI GETS IT WRONG**: "-fPIC is for shared libs, -fPIE is for executables, and
  nothing else differs", or "everything external goes through the PLT under both".
- **CORRECT REASONING**: both make code position-independent; they differ in the
  *preemption assumption*. The compiler trusts `-fPIE` that no other object interposes
  on its definitions, producing fewer indirect hops. The linker is the final decider:
  non-preemptible `PLT32` relocations are relaxed to `PC32`.
- **EXAMPLE** (bad): claiming a `-fPIE` executable can be loaded as a `.so` and used by
  another library via interposition.
- **COUNTEREXAMPLE** (good): `readelf -r` on a `-fPIE` object shows `PC32` on internal
  calls and `PLT32`/`GOTPCREL` on imported symbols; on the same source with `-fPIC`,
  the preemptible globals keep PLT/GOT forms.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — compile the same TU with `-fPIC` and `-fPIE`,
  compare `readelf -r`. PE contrast VERIFIED: no `-fPIC`/`-fPIE` needed on MinGW —
  `-fno-pic -shared` DLL builds and runs.
- **SOURCE**: `sysv-amd64-abi` (position-independent code); `gnu-ld-manual`
  (via sibling skill: `-pie`); `binutils-docs` (readelf -r).

## 10. Reading the tables at runtime with a debugger

- **RULE**: gdb exposes the dynamic machinery: `info files`/`info address` name PLT and
  GOT symbols (`foo@plt`, `__imp_foo` on PE); `set stop-on-solib-events on` breaks on
  library load; the first call to a lazy function passes through the resolver while the
  second does not; `LD_BIND_NOW=1` removes the resolver from the path entirely.
- **WHY AI GETS IT WRONG**: the agent "proves" binding behavior by reading source or by
  single-stepping into a call without checking whether the GOT slot was patched.
- **CORRECT REASONING**: the observable is the second call: disassemble the PLT stub,
  note the GOT slot's target before and after the first call (via `x/gx` on the GOT
  slot address), and correlate with `LD_DEBUG=bindings` lines.
- **EXAMPLE** (bad): "I stepped in and saw `_dl_runtime_resolve`, therefore the program
  is broken".
- **COUNTEREXAMPLE** (good): record the GOT slot value before (resolver path) and after
  (library code) the first call; that single observation confirms lazy binding.
- **VERIFICATION**: DOCUMENTED-AS-TARGET — gdb batch on an ELF host. PE analog VERIFIED:
  `objdump -d` already shows the import thunk plus `__imp_*` IAT slot; the loader fills
  the IAT at process start, which is why the thunk's target is available from the first
  call.
- **SOURCE**: `gdb-manual` (breakpoints, examining memory); `sysv-elf` (GOT/PLT layout).
