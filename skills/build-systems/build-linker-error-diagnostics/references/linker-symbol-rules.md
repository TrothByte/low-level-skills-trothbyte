# Linker Error Diagnostics — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. Linking is symbol satisfaction; enumerate symbols, not words

- **RULE**: a link succeeds iff every undefined symbol is satisfied by exactly
  one definition with the same (mangled/versioned) name. The error text names
  the unsatisfied reference; the symbol table names what exists.
- **WHY AI GETS IT WRONG**: reacts to the word "undefined" by adding code or a
  `-l` flag without ever asking which object defines the symbol.
- **CORRECT REASONING**: run `nm -u`/`objdump -t` on your objects, list the
  undefined symbols, then locate each definition. If no definition exists, add
  one (or a real library). If it exists but with a different name, that is the
  real cause.
- **EXAMPLE** (bad):
  ```
  gcc bad/undef_main.c
    ld.exe: ...:undefined reference to `missing_func'
    collect2.exe: error: ld returned 1 exit status        (exit 1)
  ```
- **COUNTEREXAMPLE** (good):
  ```
  gcc good/main.c good/lib.c   # lib.c defines missing_func
    exit 0; run exit 0
  ```
- **VERIFICATION**: recorded 2026-08-15 — link exit 1 with the error; after
  adding the definition, link exit 0 and run exit 0. `nm` on the object shows
  `U missing_func` before, `T missing_func` in the executable after.
- **SOURCE**: gnu-ld-manual (symbol resolution), binutils-docs (nm, objdump),
  sysv-elf (symbol table).

## 2. `U` / `sec 0`: the undefined-symbol row

- **RULE**: `nm` prints undefined symbols as `U` (ELF `UND`, section index 0);
  `objdump -t` prints `sec 0` rows; `readelf -s` prints `UND`. A symbol that is
  defined shows `T`/`D`/`B` (text/data/bss).
- **WHY AI GETS IT WRONG**: inspects the source instead of the symbol table and
  concludes the symbol "exists"; or greps the executable and misses that the
  symbol is still `U` in a linked object.
- **CORRECT REASONING**: symbol-table state is ground truth. `U` = reference
  without definition in this image; `T` = definition present. A versioned
  symbol (`foo@GLIBCXX_3.4.30`) has a different name than the plain one.
- **EXAMPLE** (bad):
  ```
  nm bad/undef_main.o ->  U missing_func
  ```
- **COUNTEREXAMPLE** (good):
  ```
  nm good.exe ->  00000001400014c0 T missing_func
  ```
- **VERIFICATION**: recorded 2026-08-15 — `nm` prints `U missing_func` for the
  object; `objdump -t` prints `sec 0 ... missing_func`; after a correct link
  the executable shows `T`.
- **SOURCE**: binutils-docs (nm, objdump -t), sysv-elf (UND/defined symbols).

## 3. C++ mangling: same name, different signature = different symbol

- **RULE**: C++ encodes parameter types into the symbol name (`add(int)` →
  `_Z3addi`; `add(double)` → `_Z3addd`). The linker is exact: a reference to
  `_Z3addi` is NOT satisfied by a definition of `_Z3addd`.
- **WHY AI GETS IT WRONG**: sees `add` defined in the source and reports "the
  linker is broken"; or fixes it by adding `extern "C"` to the *definition*
  only, changing nothing for a caller that expects C++ linkage.
- **CORRECT REASONING**: compare the error's demangled name with
  `nm -C`'s defined list. Recorded: `undefined reference to 'add(int)'` while
  `nm -C lib.o` shows only `T add(double)` (mangled `_Z3addd`). The fix is to
  make the definition match the reference (same parameter types / correct
  linkage), then confirm with `nm`.
- **EXAMPLE** (bad): `gcc bad/mismatch_app.cpp bad/mismatch_lib.cpp` →
  `undefined reference to 'add(int)'`, exit 1.
- **COUNTEREXAMPLE** (good): change the definition to `int add(int)` (or the
  caller to `double`) so the mangled names match; then `nm` shows a `T` for the
  referenced symbol.
- **VERIFICATION**: recorded 2026-08-15 — `nm -C lib.o` = `T add(double)`;
  raw `nm` = `T _Z3addd`; the link of caller vs that library fails.
- **SOURCE**: gnu-ld-manual, binutils-docs (nm -C demangling), gcc-manual
  (mangling).

## 4. Archives pull members lazily; --whole-archive forces them

- **RULE**: when linking an archive, the linker extracts only the member
  objects that satisfy currently-undefined symbols (plus their transitive
  needs). Unreferenced members are skipped. `--whole-archive` forces every
  member in. This is positional and per-archive.
- **WHY AI GETS IT WRONG**: expects every symbol in `lib.a` to appear in the
  binary; or "fixes" a missing symbol by reordering archives instead of checking
  which member should satisfy it.
- **CORRECT REASONING**: if a symbol lives in an archive member that nothing
  references, it is not linked. `nm libmulti.a` lists members and their
  symbols; compare with `nm app.exe` after the link. `--whole-archive` is the
  explicit way to force pull-in (e.g. for registration tables with no direct
  callers).
- **EXAMPLE** (bad):
  ```
  ar rcs libmulti.a foo.o bar.o
  gcc arch_main.o libmulti.a   # arch_main only calls foo
  nm app.exe                   # only foo present; bar absent
  ```
- **COUNTEREXAMPLE** (good):
  ```
  gcc arch_main.o -Wl,--whole-archive libmulti.a -Wl,--no-whole-archive -o w.exe
  nm w.exe                     # foo AND bar present
  ```
- **VERIFICATION**: recorded 2026-08-15 — default link pulls only `foo`; with
  `--whole-archive` both `foo` and `bar` appear in `nm w.exe`. exit 0 both.
- **SOURCE**: gnu-ld-manual (archive extraction, --whole-archive), lld-docs.

## 5. Cascades: the count is irrelevant, the prefix is the cause

- **RULE**: a link that fails with thousands of `undefined reference` lines is
  usually one structural cause (a missing generated table/object, an excluded
  archive member) producing many references. The 40,784-error kernel case
  (`__jump_table`/`__ksymtab`) was ONE missing definition source.
- **WHY AI GETS IT WRONG**: tries to fix errors one at a time; or reports "the
  linker broke" as if the linker were the faulty component.
- **CORRECT REASONING**: read the first error and the shared name prefix. All
  `__ksym_N` names trace to one generated symbol family. Fix the generator /
  include the object; then the whole cascade clears in one link.
- **EXAMPLE** (bad): `bad/cascade.c` — 12 `__ksym_N` references → 12 errors +
  `ld returned 1 exit status` (recorded; real kernel links show 40k+).
- **COUNTEREXAMPLE** (good): provide the single definition source (one object
  defining the family); one link, zero errors.
- **VERIFICATION**: recorded 2026-08-15 — 12 undefined refs each with a unique
  `.text+0x..` offset, all `__ksym_` prefixed, plus one collect2 error line.
- **SOURCE**: gnu-ld-manual, sysv-elf (symbol tables), binutils-docs.

## 6. Symbol interposition and --as-needed: shared-library semantics

- **RULE**: for shared libraries, `--as-needed` (default on many distros) skips
  DT_NEEDED entries whose symbols are not referenced; ELF interposition lets a
  definition in the main executable override a shared library's. These semantics
  are positional and platform-dependent.
- **WHY AI GETS IT WRONG**: porting a Linux `-lfoo -lbar` order to a system
  where order/as-needed differ, then chasing an undefined reference; or
  reasoning about interposition without `nm -D`/`objdump -T` evidence.
- **CORRECT REASONING**: with `--as-needed`, an unused library is dropped —
  symbols reachable only through it become undefined. With interposition, an
  override only wins if its definition is earlier in the link order. Verify with
  `nm -D`/`objdump -T` on the shared objects (ELF hosts; this MinGW host has no
  versioned GLIBCXX symbols, so this rule is target-verification).
- **EXAMPLE** (bad): dropping a `-lfoo` from the link line because "nothing
  directly calls it", then a cascade of undefined refs from `libbar`.
- **COUNTEREXAMPLE** (good): keep the library that satisfies the referenced
  symbols, or use `--no-as-needed` for that library.
- **VERIFICATION**: on this host (MinGW) the shared-lib path is documented, not
  executed; command: `nm -D libfoo.so; objdump -T libfoo.so`.
- **SOURCE**: lld-docs (--as-needed), gnu-ld-manual, sysv-elf (dynamic symbol
  tables).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Undefined ref | one unsatisfied symbol; enumerate with `nm -u` |
| `U`/`sec 0` | undefined; `T`/`D` defined — symbol table is ground truth |
| C++ mangle | `add(int)` vs `add(double)` are different symbols |
| Archives | lazy member pull-in; `--whole-archive` forces all |
| Cascades | one structural cause, shared prefix; not N bugs |
| Interposition | main-exe definitions win; order and `--as-needed` matter |
| ABI versions | `foo@VER` is a distinct name from `foo` |
| Verify fixes | `nm` on the final image shows the symbol DEFINED |
