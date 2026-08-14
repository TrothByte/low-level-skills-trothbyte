# Bad: "no symbols" means nothing to recover

Target: a rustc 1.97.1 release build (`-O`), final PE on Windows.

## The bad reading

```
$ objdump -t rpd.exe
rpd.exe:     file format pei-x86-64
SYMBOL TABLE:
no symbols
$ nm rpd.exe          (or equivalent)
no symbols
```

Agent conclusion: "Stripped Rust binary, no symbols, no markers, nothing to recover — report opaque."

## Why it is wrong (verified counter-evidence)

1. `objdump -t` showing `no symbols` is the NORMAL state of a rustc PE build. It also prints `no symbols`
   for `-g` and `-C strip=none` builds. It does not mean the binary was stripped or obfuscated.
2. The recoverable Rust markers are data, not symbols. On the same binary:

```
$ strings -a rpd_stripped.exe
double:
pick:
negative input:
 index out of bounds: the len is
called `Result::unwrap()` on an `Err` value
called `Option::unwrap()` on a `None` value
panicked at
...\rust_panic_demo.rs
```

`-C strip=symbols` removes every `_R...` name but leaves the panic corpus, the format literals, and the
source path intact (verified).

3. `objdump -s -j .rdata` locates each literal (`negative input: ` at VMA 0x140019450), and `objdump -d`
   shows the panic lowering `lea <literal>; ...; call <panic_fmt>` (verified).

## Correct approach

Treat "no symbol table" as a layer switch: strings corpus → `.rdata` literals → panic `lea`+`call` sites
→ entry flow (`lang_start`). On ELF, keep `nm`/`objdump -t` as a first pass and `rustfilt`/`nm -C` for
demangling — but never conclude "opaque" from the PE symbol table alone.
