# Bad methodology — Go and Rust binary RE

The bad-practice examples show the reasoning mistakes the reference rules target. Each is paired with the
verified or documented counter-evidence.

## rust_strip_misjudgment.md — "no symbols" read as "nothing to recover"

`objdump -t` on the final rustc PE prints `SYMBOL TABLE: no symbols`. The bad reasoning stops there and
reports the binary as opaque. The good methodology switches layers: the panic corpus and `core::fmt`
literals live in `.rdata` as data and survive `-C strip=symbols` (verified), and mangled `_R...` names
are visible at the object layer (`rustc --emit=obj`, verified).

## go_pclntab_misread.md — reading `.gopclntab` as a plain symbol table

The pclntab is a versioned, PC-indexed table. Reading it linearly — or applying a Go 1.16 field layout to
a Go 1.20+ binary — produces wrong `funcnametab`/`functab` offsets and misrecovered names. The magic
(0xfffffffb/fa/f0/f1) must select the layout first, and entry PCs are offsets relative to `textStart`
since Go 1.18 (documented-as-target, per `src/internal/abi/symtab.go`).
