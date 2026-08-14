# Evaluation — go-rust-re

Skill: `skills/reverse-engineering/go-rust-re`. Toolchain: rustc 1.97.1 (pe-x86-64), GNU binutils 2.46
(objdump/strings), Windows. Go toolchain absent: Go statements are `documented-as-target`, grounded in the
Go runtime sources cited in the reference. Claims marked VERIFIED below were exercised in this run.

## Synthetic evals

- **easy/positive**: identify a Go binary from `.gopclntab` magic + `.go.buildinfo` and recover
  `main.main` after `strip -s` (documented-as-target, Rule 1-3).
- **easy/positive**: on `examples/good/rust_panic_demo.rs` (`-O`), find the panic string `negative input: `
  in `.rdata`, the `lea` + `call <panic_fmt>` site in `.text`, and `core::panicking` references in the
  object symbols (Rule 8-10). VERIFIED.
- **medium/negative**: stripped Rust PE (`-C strip=symbols`) — recover the panic corpus and format
  literals despite `objdump -t` printing `no symbols` (Rule 9). VERIFIED.
- **medium/negative**: classify `_RNvNtCs55qC6OcLGgs_4core9panicking9panic_fmt` as v0-mangled Rust, not
  C++ (Rule 8, 11). VERIFIED.
- **hard/negative**: read a `dyn Trait` call as an indirect vtable call (data ptr + vtable ptr, slot 0 =
  drop glue) instead of a direct call (Rule 12, documented-as-target).
- **hard/adversarial**: Go 1.16 (`0xfffffffa`) vs Go 1.20+ (`0xfffffff1`) pclntab layout misparse — the
  magic must select the field layout (Rule 2, documented-as-target).
- **adversarial**: a Go binary where SysV AMD64 arg decoding mislabels the first argument because Go uses
  ABIInternal (AX, BX, CX, DI, SI, R8, R9), not RDI/RSI/RDX (Rule 6, documented-as-target).

## False-positive evals (correct analysis must not be flagged)

- A C++ binary with `_ZN...Ev` symbols must NOT be reported as Rust solely because names start with `_ZN`
  (no `17h<hash>`, no `core`/`std` crates). (Rule 11)
- A Go binary with an intact `.gopclntab` must NOT be reported as "stripped and opaque" when `.symtab` is
  absent. (Rule 1)
- A binary whose `.rdata` contains panic strings must NOT be reported as "no Rust markers" just because
  the symbol table is empty. (Rule 9)
- A normal rustc PE showing `objdump -t: no symbols` must NOT be described as "stripped build"; that is
  the toolchain default on this target. (Rule 8)

## Verification commands (executed on this host)

```
rustc -O examples/good/rust_panic_demo.rs -o rpd.exe
rustc -O -g examples/good/rust_panic_demo.rs -o rpd_dbg.exe
rustc -O -C strip=none examples/good/rust_panic_demo.rs -o rpd_stripnone.exe
rustc -O -C strip=symbols examples/good/rust_panic_demo.rs -o rpd_stripped.exe
rustc -O --emit=obj examples/good/rust_panic_demo.rs -o rpd.obj
rustc -O --emit=obj -C symbol-mangling-version=v0 examples/good/rust_panic_demo.rs -o rpd_v0.obj
objdump -t rpd.exe rpd_dbg.exe rpd_stripnone.exe      # all print "no symbols"
objdump -t rpd_v0.obj | findstr _RNv                   # v0 mangled names
objdump -s -j .rdata rpd_dbg.exe | findstr negative    # panic literal at 0x140019450
objdump -d rpd_dbg.exe | findstr 140019450             # lea -> panic_fmt call site
objdump -p rpd.exe | findstr "DLL Name"                # VCRUNTIME140 / api-ms-win-crt-*
strings -a rpd_stripped.exe | findstr "negative input" # survives stripping
rpd.exe -5; rpd.exe 2 9; rpd.exe 2 1                   # panic exit 101 / normal exit 0
```

Documented-as-target (Go host / ELF):

```
go version -m bin
objdump -s -j .gopclntab bin
GoReSym -t -d -p -strings bin
go tool objdump -s 'main\.main' bin
dlv exec bin -- goroutines
```

## Verified facts (rustc 1.97.1, pe-x86-64, Windows)

| Fact | Command | Observed |
|---|---|---|
| default mangling is v0 (`_R...`) on this target | `objdump -t rpd.obj` | `_RNvCs1wWDqIOd2eB_15rust_panic_demo4main`; `_RNvNtCs55qC6OcLGgs_4core9panicking9panic_fmt` |
| explicit v0 also `_R...` | `-C symbol-mangling-version=v0` | `_RNvCs1wWDqIOd2eB_15rust_panic_demo4main` |
| `-C symbol-mangling-version=legacy` unavailable on stable | build | error: requires `-Z unstable-options` |
| final PE has no COFF symbol table (default, `-g`, `strip=none`) | `objdump -t` | `SYMBOL TABLE: no symbols` |
| panic string in `.rdata` | `objdump -s -j .rdata rpd_dbg.exe` | `negative input: ` at VMA `0x140019450` |
| panic lowering in `.text` | `objdump -d rpd_dbg.exe` | `lea 0x17c4a(%rip),%rcx # 0x140019450`; `call 0x140018490` (panic_fmt) |
| OOB message pieces in `.rdata` | `strings -a` | ` index out of bounds: the len is ` |
| `-C strip=symbols` keeps strings | `strings -a rpd_stripped.exe` | `negative input: `, `double: `, `pick: `, source path `rust_panic_demo.rs` |
| panic runtime behavior | `rpd.exe -5` | `panicked at ...rust_panic_demo.rs:26:9: negative input: -5`, exit 101 |
| OOB runtime behavior | `rpd.exe 2 9` | `index out of bounds: the len is 3 but the index is 9`, exit 101 |
| normal runtime behavior | `rpd.exe 2 1` | `double: 4`, `pick: 20`, exit 0 |
| Rust-on-Windows imports | `objdump -p rpd.exe` | KERNEL32.dll, ntdll.dll, VCRUNTIME140.dll, api-ms-win-crt-* |

## Panic string corpus (for eval assertions)

- `negative input: {n}` (user `panic!` with argument)
- `index out of bounds: the len is {len} but the index is {idx}` (OOB slice index; static fragments
  ` index out of bounds: the len is ` / ` but the index is ` in `.rdata`)
- `called \`Result::unwrap()\` on an \`Err\` value: ...`
- `called \`Option::unwrap()\` on a \`None\` value`
- `panicked at {path}:{line}:{col}:` (runtime panic header)
- `aborting due to panic at ...`, `thread caused non-unwinding panic. aborting.` (abort paths)
