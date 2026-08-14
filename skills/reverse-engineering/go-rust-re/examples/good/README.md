# Good methodology — Go and Rust binary RE

## Rust: verified walkthrough (rustc 1.97.1, pe-x86-64, objdump 2.46)

Source: `rust_panic_demo.rs`. Build and inspect:

```
rustc -O rust_panic_demo.rs -o rpd.exe
rustc -O -C strip=symbols rust_panic_demo.rs -o rpd_stripped.exe
rustc -O --emit=obj rust_panic_demo.rs -o rpd.obj
rustc -O --emit=obj -C symbol-mangling-version=v0 rust_panic_demo.rs -o rpd_v0.obj
```

Step 1 — symbols on the final PE. `objdump -t rpd.exe` prints `file format pei-x86-64` then `SYMBOL TABLE:
no symbols`. This is the NORMAL state of a rustc PE build (verified also with `-g` and `-C strip=none`): the
COFF symbol table is not emitted. "No symbols" is NOT a failure signal; it is a signal to switch layers.

Step 2 — mangled names live in the object layer. `objdump -t rpd.obj` shows v0-mangled symbols:

```
_RNvCs1wWDqIOd2eB_15rust_panic_demo4main
_RNvNtCs55qC6OcLGgs_4core9panicking9panic_fmt
_RNvNtCs55qC6OcLGgs_4core9panicking18panic_bounds_check
_RNvNtCs..._3std2rt19lang_start_internal
```

Both the default build and `-C symbol-mangling-version=v0` emit `_R`-prefixed (v0) symbols on this target;
`-C symbol-mangling-version=legacy` requires nightly. On ELF targets the classic default is legacy
`_ZN...17h<hash>E` (documented-as-target).

Step 3 — strings survive stripping. `strings -a rpd_stripped.exe` still contains:

```
double:
pick:
negative input:
 index out of bounds: the len is
called `Result::unwrap()` on an `Err` value
called `Option::unwrap()` on a `None` value
...\rust_panic_demo.rs
```

Step 4 — locate the panic path in `.rdata` and `.text`:

```
objdump -s -j .rdata rpd.exe     # "negative input: " at VMA 0x140019450
objdump -d rpd.exe               # find the lea referencing 0x140019450
```

Observed call site (`panic!("negative input: {}", x)`):

```
1400017e7: test %rcx,%rcx
1400017ea: jns  0x1400015bd          ; x >= 0 -> skip panic
1400017f4: lea  0x14335(%rip),%rax   # 0x140015b30  (fmt::Arguments / format info)
1400017ff: lea  0x17c4a(%rip),%rcx   # 0x140019450  ("negative input: " in .rdata)
14000180d: mov  %rbp,%rdx
140001810: call 0x140018490          ; -> core::panicking::panic_fmt
```

This is the canonical Rust panic lowering: format-literal pointer + Arguments descriptor + payload,
then a single call into `core::panicking`.

Step 5 — runtime behavior: `rpd.exe -5` prints `thread 'main' panicked at ...rust_panic_demo.rs:26:9:
negative input: -5` and exits 101; `rpd.exe 2 9` prints `double: 4` then panics
`index out of bounds: the len is 3 but the index is 9` (exit 101); `rpd.exe 2 1` prints `double: 4`,
`pick: 20`, exit 0.

Step 6 — Rust-on-Windows import fingerprint: `objdump -p rpd.exe` shows imports from KERNEL32.dll,
ntdll.dll, VCRUNTIME140.dll and api-ms-win-crt-* DLLs (not plain libc) — a distinguishing signal versus a
pure C build.

## Go: annotated pclntab reading (documented-as-target)

No Go toolchain on this host; byte values below are representative of a Go 1.22 amd64 ELF build and the
field order is the Go 1.20+ layout per `src/internal/abi/symtab.go` and `src/debug/gosym/pclntab.go`.
Re-verify on a real target before relying on offsets.

```
$ objdump -s -j .gopclntab demo_go            (documented-as-target)
00000000 f1ffffff 00000108 34120000 00000000  <- magic 0xfffffff1 (Go 1.20+), pad, quantum=1, ptr=8
                                                nfunctab (uint64, LE)
00000010 2c010000 00000000 00000000 00000000  <- nfiletab, textStart (0 here: use runtime.text)
00000020 78000000 00000000 88000000 00000000  <- funcnametab off, cutab off
00000030 90000000 00000000 a0000000 00000000  <- filetab off, pctab off
00000040 10010000 00000000                    <- funcdata/functab off
```

Reading sequence: (1) magic `0xfffffff1` selects the Go 1.20+ layout; (2) `funcnametab` holds
NUL-terminated names such as `main.main`, `runtime.main`, `main.(*Type).Method`; (3) `functab` is
(pcOffset, funcOff) pairs, each funcOff resolved against `funcdata`, and for Go >= 1.18 entry PCs are
offsets added to `textStart`; (4) `pctab` holds varint-encoded PC→(file,line,value) streams used for
line numbers. GoReSym does all of this across every layout >= Go 1.2, including stripped and UPX-packed
binaries, and is the recommended first tool; `redress` (goretk) targets stripped Go binaries for the
decompile pass.
