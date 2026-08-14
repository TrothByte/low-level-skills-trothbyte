# Bad: treating `.gopclntab` as a plain symbol table

Target: a stripped Go binary (Go 1.22, amd64, ELF). All Go statements here are `documented-as-target`
(no Go toolchain on this host); field names and magic values follow `src/internal/abi/symtab.go` and
`src/debug/gosym/pclntab.go`.

## The bad reading

Agent finds the `.gopclntab` section, scans it with a C-string extractor, and treats every NUL-terminated
run as "a symbol whose address is its offset in the section":

```
$ objdump -s -j .gopclntab demo_go
00000000 fbffffff ...     <- guessed "magic is always 0xfffffffb"
...
$ strings -t x -j .gopclntab demo_go
    ... main.main
Agent: "main.main is a symbol at offset X in .gopclntab"
```

## Why it is wrong

1. The magic is version-dependent: `0xfffffffb` (Go 1.2-1.15), `0xfffffffa` (1.16-1.17), `0xfffffff0`
   (1.18-1.19), `0xfffffff1` (1.20+). A Go 1.22 binary uses `0xfffffff1`; reading it as `0xfffffffb`
   selects the wrong layout and every field offset after byte 7 is wrong.
2. The pclntab is not a name→address table. It is `functab` (PC, funcOff) pairs resolved against
   `funcdata` records whose `nameOff` indexes `funcnametab`; names recovered by linear string scanning
   have no reliable address mapping and no line data.
3. Since Go 1.18 the `funcdata` entry PC is a 32-bit offset added to `textStart`, not an absolute address;
   naive readers subtract the section base and get wrong targets.
4. `strings`-style extraction also conflates `.go.buildinfo`, type-name strings, and function names that
   are all resident in read-only data.

## Correct approach

Read the magic first, select the layout, then resolve `functab`/`funcdata`/`funcnametab` exactly as
`src/debug/gosym/pclntab.go` does — or delegate to GoReSym, which implements that parser for every layout
>= Go 1.2 including stripped and UPX-packed binaries, and to redress (goretk) for the decompile pass.
