# Go and Rust Binary Reverse Engineering

Each entry: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE → VERIFICATION → SOURCE.
Claims marked `VERIFIED` were exercised on this host (rustc 1.97.1, pe-x86-64, objdump 2.46). Claims marked
`documented-as-target` describe behavior of a toolchain not present on this host (Go) or of ELF builds, grounded
in the cited primary sources; they must be re-verified on the target before use in a stable skill.

## 1. `.gopclntab` is a runtime symbol table, not an ELF symbol table — and it survives `strip`

- **RULE**: A Go binary embeds a self-describing PC→function table, the pclntab, in the `.gopclntab` section.
  The Go runtime uses it for tracebacks, profiling, and stack unwinding. It stores full function names, source
  file names, and line numbers. The classic ELF symbol table (`.symtab`/`.strtab`) can be removed entirely
  (`strip -s`, `-ldflags="-s -w"`) without touching the pclntab, so a "stripped" Go binary still exposes every
  function name. `documented-as-target`.
- **WHY AI GETS IT WRONG**: agents run `strip`/`nm`, see "no symbols", and conclude the binary is opaque; or
  they treat `.gopclntab` as an ordinary symbol table and are confused when `objdump -t` shows nothing while
  `strings` reveals `main.main` and `runtime.main`.
- **CORRECT REASONING**: classify the pclntab as runtime metadata indexed by PC, not linker symbol data. When a
  Go binary looks "stripped", parse `.gopclntab` with a Go-aware tool (GoReSym, gore, Delve) and recover
  function names, addresses, and source files without `.symtab`.
- **EXAMPLE** (bad): "The binary has no `.symtab`, so no functions can be recovered." — false for Go.
- **COUNTEREXAMPLE** (good): `GoReSym -t -d -p bin` outputs `UserFunctions` such as `main.main`,
  `runtime.main`, and `main.(*Type).Method` with start/end addresses, all read from the pclntab after stripping.
- **VERIFICATION**: on a Linux host, `strip -s` a Go ELF binary, confirm `nm` shows nothing, then GoReSym still
  lists every function. Not runnable on this host (no Go toolchain) — `documented-as-target`.
- **SOURCE**: `binutils-docs` (objdump/readelf/nm), `sysv-elf` (ELF symbol table vs runtime metadata),
  `dwarf-v5` (Go emits DWARF by default; another recovery path unless `-w`).

## 2. The pclntab header encodes the Go version and selects the layout — read the magic first

- **RULE**: The pclntab begins with a 4-byte magic that fixes the table layout. Go's `src/internal/abi/symtab.go`
  defines: `0xfffffffb` Go 1.2–1.15; `0xfffffffa` Go 1.16–1.17 (header fields added); `0xfffffff0` Go 1.18–1.19
  (function entry PCs became offsets); `0xfffffff1` Go 1.20+ (colon added to generated symbol names). The header
  continues: two zero bytes, pc quantum (1/2/4), pointer size (4/8), then pointer-sized words holding counts and
  offsets (nfunctab, nfiletab, textStart, then funcnametab/cutab/filetab/pctab/funcdata; the field order changed
  at 1.16 and 1.18). `funcnametab` is NUL-terminated names; `functab` is (pc, funcOff) pairs; `pctab` is
  varint-encoded PC→(file/line/value) streams; `funcdata` holds `_func` records (nameOff, pcfile, pcln, cuOff).
  `documented-as-target`.
- **WHY AI GETS IT WRONG**: agents read the pclntab like a linear symbol table (offset → name) and misparse the
  index tables; or they quote wrong magic values from memory — wrong version → wrong field layout → garbage.
- **CORRECT REASONING**: version-first parsing: read the magic, select the layout, then resolve `nameOff`/`entryOff`
  against the sub-table bases. For Go ≥ 1.18 function entries are offsets relative to `textStart`, not absolute PCs.
- **EXAMPLE** (bad): "The pclntab magic is always `0xFFFFFFF1`." Only Go ≥ 1.20 uses it; 1.18–1.19 is
  `0xfffffff0`, 1.16–1.17 is `0xfffffffa`, 1.2–1.15 is `0xfffffffb`.
- **COUNTEREXAMPLE** (good): read magic → pick layout → resolve `funcnametab` and recover `main.main`; the parsed
  `TabMeta.Version` agrees with the version in `.go.buildinfo`.
- **VERIFICATION**: `objdump -s -j .gopclntab bin | head` shows the magic; compare against
  `src/internal/abi/symtab.go`; GoReSym handles all layouts ≥ 1.2. `documented-as-target` on this host.
- **SOURCE**: `binutils-docs` (objdump section dumps), `gdb-manual` (reading memory/strings), `dwarf-v5`.

## 3. `.go.buildinfo` names the exact Go version and module — read it before parsing anything

- **RULE**: Go binaries carry a `.go.buildinfo` section starting with the magic `\xff Go buildinf:`, containing
  the Go toolchain version, module path, module checksums, and the BuildID. `go version -m bin` prints it.
  The version selects the pclntab layout and the type-parsing rules, so it is the first triage step. `documented-as-target`.
- **WHY AI GETS IT WRONG**: agents guess the Go version from the presence of `runtime.main` and then misparse
  moduledata types; GoReSym type parsing explicitly fails if the version is wrong and needs `-v` to override.
- **CORRECT REASONING**: recover the exact version from `.go.buildinfo` first; every later step (layout choice,
  type recovery) depends on it. On a stripped binary whose buildinfo is intact this is fast and exact.
- **EXAMPLE** (bad): parsing types with an assumed Go 1.16 layout for a Go 1.22 binary — type offsets are wrong.
- **COUNTEREXAMPLE** (good): `go version -m bin` → `go1.22.6` → GoReSym auto-detects and parses the 1.20+ layout.
- **VERIFICATION**: `go version -m bin`; `strings -a bin | findstr "Go buildinf"`. Not runnable here (no Go
  toolchain) — `documented-as-target`.
- **SOURCE**: `binutils-docs` (strings/objdump), `sysv-elf` (custom sections).

## 4. Go strings are length-prefixed (ptr, len) and not NUL-terminated

- **RULE**: A Go string value is a two-word header — data pointer and length — and the bytes are UTF-8 with no
  NUL terminator. String literals live in `.rodata` and are deduplicated (interned). Tools that treat them as C
  strings truncate at the first `\x00`, mis-count embedded NULs, or mistake the pointer word for the string.
  `documented-as-target`.
- **WHY AI GETS IT WRONG**: agents model Go strings as C strings; they trim at the first NUL, miss embedded NULs,
  and misread a `lea` of a pointer into `.rodata` as "the string is here" without accounting for the separate
  length operand or header word.
- **CORRECT REASONING**: treat Go strings as (ptr, len). When disassembling, a string pass usually pairs a pointer
  load with a length in another register/immediate; the runtime and gdb print Go strings by length.
- **EXAMPLE** (bad): `objdump -s .rodata` and reading `"payload\x00\x01..."` as a C string, losing everything after
  the NUL and mislabeling the length.
- **COUNTEREXAMPLE** (good): GoReSym `-strings` extracts strings from the internment table, and GDB's Go language
  support prints `string` values correctly via their length.
- **VERIFICATION**: on a Go binary, compare `strings -a` boundaries against GoReSym string entries; lengths agree
  only when read as (ptr, len). `documented-as-target`.
- **SOURCE**: `binutils-docs` (strings), `gdb-manual` (Go string type support).

## 5. Goroutine stacks are user-space, small, and growable — one OS thread carries many stacks

- **RULE**: Go schedules goroutines onto OS threads (M:N scheduler). A goroutine stack starts at ~2 KiB and grows
  on demand; stack bounds live in the `g` struct (`stack.lo`/`stack.hi`). There is not one stack per OS thread.
  GDB has built-in Go language support (string/type printing); Delve (`dlv`) is the Go-native debugger with
  `goroutines`, `stack`, and `bt` commands. `documented-as-target`.
- **WHY AI GETS IT WRONG**: agents assume OS threads each have one fixed stack and try classic stack unwinding; on
  a crash they inspect the thread stack and miss the goroutine stack and `goid`.
- **CORRECT REASONING**: use Go-aware unwinders (dlv, gdb with Go scripts, gore) that read the `g` struct and
  pclntab; goroutine IDs and per-goroutine stacks come from the runtime, not from OS thread state.
- **EXAMPLE** (bad): reading a panic traceback only from the OS thread and missing the goroutine's `goid` and
  stack bounds.
- **COUNTEREXAMPLE** (good): `dlv goroutines` lists goroutines with IDs; `dlv stack` unwinds the selected
  goroutine's own stack.
- **VERIFICATION**: trigger a Go panic and confirm tracebacks list goroutine IDs; `documented-as-target`.
- **SOURCE**: `gdb-manual` (Go language support), `binutils-docs`.

## 6. Go's calling convention is not the SysV AMD64 one — do not decode args with `sysv-amd64-abi` rules

- **RULE**: Since Go 1.17 the amd64 default internal ABI (ABIInternal) passes integer/pointer args in AX, BX, CX,
  DI, SI, R8, R9 and returns in AX, BX, CX; FP args in X0–X14. That differs from SysV AMD64
  (RDI, RSI, RDX, RCX, R8, R9) and from Windows x64 (RCX, RDX, R8, R9). Go also uses a frame pointer and no red
  zone. ABIExternal (cgo/C interop) is the only path that looks SysV/Windows-like. `documented-as-target`.
- **WHY AI GETS IT WRONG**: agents apply SysV AMD64 ABI rules to Go functions and mislabel arguments — e.g. reading
  the first argument from `%rdi` when ABIInternal placed it in AX.
- **CORRECT REASONING**: distinguish ABIInternal (Go→Go, register convention above) from ABIExternal (cgo/C);
  decode Go function signatures by ABIInternal register order and confirm with GoReSym/redress or
  `go tool objdump`.
- **EXAMPLE** (bad): decoding `runtime.main`'s argument under SysV from `%rdi`.
- **COUNTEREXAMPLE** (good): `main.main` arguments are passed in AX/BX/CX on amd64 ABIInternal; `go tool objdump`
  shows the register moves.
- **VERIFICATION**: `go tool objdump -s 'main\.main' bin` on a Go host. `documented-as-target`.
- **SOURCE**: `sysv-amd64-abi` (the convention you must NOT apply to ABIInternal), `gdb-manual`.

## 7. Use the GoReSym → redress recovery pipeline instead of hand-parsing the pclntab

- **RULE**: For Go binaries the production workflow is: (1) confirm Go via the `.gopclntab` magic and
  `.go.buildinfo`; (2) extract metadata with GoReSym (`GoReSym -t -d -p -strings bin`), which mirrors the Go
  compiler/runtime parsers and returns JSON with Version, BuildId, Arch, TabMeta, ModuleMeta, Types, Files,
  Strings, UserFunctions, StdFunctions; (3) apply results in IDA/Ghidra (`goresym_rename.py`); redress (goretk)
  is a purpose-built analyzer for stripped Go binaries. GoReSym handles layouts ≥ 1.2, stripped, malformed, and
  UPX-packed binaries. `documented-as-target`.
- **WHY AI GETS IT WRONG**: agents hand-parse the pclntab from raw hexdumps and miss version/layout branches, or
  reinvent name recovery instead of using maintained parsers that track every Go release.
- **CORRECT REASONING**: use tools based directly on the Go runtime sources; treat GoReSym output as ground truth
  for names/types/strings and cross-check addresses with objdump.
- **EXAMPLE** (bad): hand-computing `funcnametab` offsets on a Go 1.21 binary using a Go 1.16 layout — wrong for
  every function.
- **COUNTEREXAMPLE** (good): `GoReSym -t -d -p -strings bin > meta.json`, then
  `goresym_rename.py meta.json` in IDA, then cross-verify function entries with `objdump -d`.
- **VERIFICATION**: GoReSym's `TabMeta.Version` matches `.go.buildinfo`; function addresses land inside `.text`.
  `documented-as-target`.
- **SOURCE**: `binutils-docs` (objdump/readelf), `dwarf-v5` (Go DWARF fallback for names/lines).

## 8. Rust mangling: legacy `_ZN...17h<hash>E` vs v0 `_R...` — and the default depends on the target

- **RULE**: Rust symbols are either legacy-mangled (`_ZN<len>crate<len>item...17h<hash>E`, Itanium-derived) or v0-
  mangled (`_R...`, nested `N` paths, `C` crate roots, backrefs, disambiguators). VERIFIED on rustc 1.97.1
  (pe-x86-64): the default is v0 — object symbols are `_RNvCs1wWDqIOd2eB_15rust_panic_demo4main` and
  `_RNvNtCs..._4core9panicking9panic_fmt`; `-C symbol-mangling-version=legacy` requires nightly on this
  toolchain. On ELF targets (documented-as-target) legacy `_ZN...17h...` is the classic default, e.g.
  `_ZN4core9panicking13panic_fmt17h...E`.
- **WHY AI GETS IT WRONG**: agents assume Rust is always `_ZN...`, confuse Rust's `_ZN...17h<hash>E` with C++
  `_ZN...Ev`, or fail to recognize v0 `_R...` as Rust at all.
- **CORRECT REASONING**: recognize both schemes. Legacy Rust names contain `17h<hex>` hash components and crates
  like `core`/`std`; v0 names start `_R` and contain `Cs<dis>_<crate>`. On PE the final exe may have no symbol
  table at all (see Rule 9) — then names are unrecoverable and you switch to strings/panic paths.
- **EXAMPLE** (bad): "Rust mangling is always `_ZN...`" — VERIFIED false on this toolchain's default
  (pe-x86-64, v0).
- **COUNTEREXAMPLE** (good): `rustc --emit=obj` then `objdump -t` shows `_RNv...` symbols; on ELF, `nm -C`/
  `rustfilt` demangle both schemes.
- **VERIFICATION**: `objdump -t` on `rpd_v0.obj` lists `_RNvCs..._15rust_panic_demo4main`; default and v0 builds
  both emit `_R`-prefixed symbols here. `VERIFIED`.
- **SOURCE**: `rust-reference` (items/functions.html), `binutils-docs`, `sysv-amd64-abi`.

## 9. Panic infrastructure: `rust_begin_unwind`, `core::panicking`, and panic strings survive stripping

- **RULE**: A Rust panic compiles to a call into `core::panicking` — `panic_fmt` for `panic!`, `panic_bounds_check`
  for out-of-bounds indexing — reached via the `rust_begin_unwind` lang item. Panic message literals are data in
  `.rdata`/`.rodata`. VERIFIED (rustc 1.97.1, `-O`): `panic!("negative input: {}", x)` lowers to a `lea` of the
  string (`.rdata` VMA `0x140019450`) followed by `call <panic_fmt>`; the OOB path embeds ` index out of bounds:
  the len is `; a `-C strip=symbols` build removes every `_R...` name but keeps the panic strings, the format
  literals, and the source path `rust_panic_demo.rs`.
- **WHY AI GETS IT WRONG**: agents look only at symbols; when symbols are stripped they declare the binary has no
  Rust markers, missing that panic strings and format templates are read-only data, not symbols.
- **CORRECT REASONING**: in stripped Rust binaries hunt the panic corpus in `.rdata` — `index out of bounds: the
  len is`, `called \`Result::unwrap()\` on an \`Err\` value`, `panicked at`, `aborting due to panic` — and
  cross-reference each string address back to its `lea`/`call` site in `.text`.
- **EXAMPLE** (bad): `-C strip=symbols` build reported as "no Rust markers". VERIFIED false: `strings -a` still
  shows `negative input: `, ` index out of bounds: the len is `, `double: `, `pick: `, and the source path.
- **COUNTEREXAMPLE** (good): on the stripped PE, `strings -a` yields the panic corpus; `objdump -s -j .rdata`
  locates each literal; `objdump -d` shows `lea <string>; ...; call <panic_fmt>` at the panic site.
- **VERIFICATION**: `strings -a rpd_stripped.exe | findstr "negative input"` and
  `objdump -d rpd_dbg.exe | Select-String 140019450`. `VERIFIED`.
- **SOURCE**: `rust-reference` (panic.html), `rustonomicon` (unwinding/panicking), `binutils-docs`.

## 10. `core::fmt` machinery: format strings are templates with argument tables, not plain strings

- **RULE**: `println!`/`format!`/`panic!` with `{}` build a `core::fmt::Arguments` — the format literal plus a
  slice of `ArgumentV1` entries (each a value pointer + a `Display`/`Debug` formatter function pointer) — and call
  `core::fmt::write`. The literal sits in `.rdata`/`.rodata` with its argument descriptor table adjacent. VERIFIED:
  `.rdata` holds `double: ` and `pick: ` literals; the disassembly shows `lea` of the Arguments pointer +
  `call` into the fmt machinery.
- **WHY AI GETS IT WRONG**: agents report the format literal as a standalone constant and miss that it is a
  template whose `{}` slots are filled through a function-pointer table.
- **CORRECT REASONING**: reconstruct formatted output by pairing the literal with its argument table; each `{}`
  maps to an `ArgumentV1` slot, and the formatter pointers identify which trait (`Display` vs `Debug`) fills it.
- **EXAMPLE** (bad): labeling `double: ` an "unused string constant" instead of a `{}` template.
- **COUNTEREXAMPLE** (good): `double: {}` template + adjacent ArgumentV1 table + `core::fmt` call → the printed
  value is the argument.
- **VERIFICATION**: `objdump -s -j .rdata` for the literals; `objdump -d` for the `call` into fmt machinery.
  `VERIFIED`.
- **SOURCE**: `rust-reference` (panic.html, fmt machinery), `binutils-docs`.

## 11. Distinguishing Rust from C/C++ in disassembly

- **RULE**: Rust markers: v0/legacy mangled names (`_R...`, `_ZN...17h...`), `core::fmt::Arguments` handling,
  panic strings, and `std::rt::lang_start`/`lang_start_internal` wrapping `main`. On Windows, Rust std links
  KERNEL32/ntdll plus VCRUNTIME140.dll and api-ms-win-crt-* imports, not plain libc. VERIFIED: the built PE
  imports `VCRUNTIME140.dll` and `api-ms-win-crt-*`; object symbols include
  `_RNvNtCs..._3std2rt19lang_start_internal` and `_RNvNtCs..._3std3env4args`.
- **WHY AI GETS IT WRONG**: agents see `_ZN...` and say "C++", or see a static-looking small binary and say "Go".
  C++ uses `_ZN...Ev`-style names with no `h<hash>`; Go names use dots (`main.main`) and live in `.gopclntab`.
- **CORRECT REASONING**: combine signals: mangling scheme + hash suffix + crate names (`core`/`std`) + panic
  corpus + `lang_start` entry flow ⇒ Rust. Presence of `core::panicking` references or `rust_begin_unwind`
  (ELF) is conclusive.
- **EXAMPLE** (bad): `_ZN3std2io5stdio6print...` mislabeled as C++ because it starts with `_ZN`.
- **COUNTEREXAMPLE** (good): `17h...` hash + `core`/`std` crates + `panicked at` strings ⇒ Rust.
- **VERIFICATION**: `objdump -t`/`-p` on the exe and objects; `strings -a` for the panic corpus. `VERIFIED`.
- **SOURCE**: `rust-reference`, `binutils-docs`, `sysv-amd64-abi`, `sysv-elf`.

## 12. Rust trait objects are fat pointers — reconstruct vtable calls as indirect calls

- **RULE**: A `dyn Trait` reference is a fat pointer: two qwords — data pointer and vtable pointer. The vtable
  lives in `.data.rel.ro`/`.rdata` and holds drop glue, size, align, then one method pointer per trait method in
  declaration order. A trait call loads the slot and does `call *slot`. `documented-as-target` (no live `dyn`
  sample on this host).
- **WHY AI GETS IT WRONG**: agents treat trait-object calls like direct calls, or read only the first qword of
  the fat pointer and lose the vtable address.
- **CORRECT REASONING**: recognize the two-consecutive-qword load (data, vtable); follow the vtable; slot 0 is
  drop glue (`drop_in_place`), then methods in trait order; name them from ELF symbols or a recovered type table.
- **EXAMPLE** (bad): decoding a fat pointer as one 8-byte value and guessing a direct call target.
- **COUNTEREXAMPLE** (good): vtable slot 0 = drop glue, slot 1..n = trait methods; call site is an indirect call
  through the slot.
- **VERIFICATION**: on ELF, `nm` shows `_ZN...drop_in_place...` and vtable symbols; on PE, use Ghidra struct
  recovery and confirm with panic strings. `documented-as-target`.
- **SOURCE**: `rust-reference` (type-layout.html, trait objects), `sysv-amd64-abi` (pointer/address layout).

## Quick triage table

| Signal | Reads | Tool | Rule |
|---|---|---|---|
| `.gopclntab` magic + `.go.buildinfo` | Go version, layout | objdump/readelf, GoReSym | 2, 3 |
| Stripped Go ELF, "no symbols" | Full function list | GoReSym, redress | 1, 7 |
| `main.main` names with dots | Function names | GoReSym, `go tool objdump` | 1, 7 |
| 2-word string headers, no NUL | String literals | GoReSym `-strings`, gdb | 4 |
| `_R...` / `_ZN...17h...` names | Rust crate/fn layout | objdump/nm, rustfilt | 8 |
| ` index out of bounds`, `panicked at` | Panic paths | `strings -a`, objdump -s | 9, 10 |
| `VCRUNTIME140`/`api-ms-win-crt-*` imports | Rust-on-Windows | objdump -p | 11 |
| `mov (vtable),r; call *r` on fat ptr | Trait dispatch | ghidra/objdump | 12 |
