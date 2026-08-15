# Evaluation — build-linker-error-diagnostics

Skill: `skills/build-systems/build-linker-error-diagnostics`.
Toolchain: GCC 16.1.0 + GNU ld (collect2) 2.46, nm/objdump/ar 2.46 (MSYS2
ucrt64, PE/COFF objects). `readelf` cannot parse COFF on this host (records
below); on ELF hosts it is the equivalent of objdump -t. All commands recorded
2026-08-15.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/undef_main.c` | `undefined reference to 'missing_func'` | exit 1 |
| medium/negative | `bad/mismatch_app.cpp` + `bad/mismatch_lib.cpp` | `undefined reference to 'add(int)'` | exit 1 |
| medium/negative | `bad/cascade.c` | 12 refs, one prefix, one cause | exit 1, 13 lines |
| easy/positive | `good/main.c` + `good/lib.c` | link + run | exit 0 |
| medium/positive | `good/arch_main.c` + `libmulti.a` | lazy pull-in; nm evidence | exit 0 |

## Actual verification runs (recorded 2026-08-15)

```
gcc bad/undef_main.c
  ld.exe: ...main.c:(.text+0x13): undefined reference to `missing_func'
  collect2.exe: error: ld returned 1 exit status
  exit 1

nm main.o                          ->   U missing_func
objdump -t main.o                  ->  [ 17](sec 0)(... ) 0x0000000000000000 missing_func
readelf -s main.o
  readelf: Error: Not an ELF file - it has the wrong magic bytes at the start
  # COFF host; readelf is the ELF-host tool. objdump -t is the equivalent here.

gcc -c bad/mismatch_lib.cpp -o lib.o
nm -C lib.o                        ->  0000000000000000 T add(double)
nm lib.o                           ->  0000000000000000 T _Z3addd
gcc bad/mismatch_app.cpp bad/mismatch_lib.cpp
  ld.exe: ...app.cpp:(.text+0x13): undefined reference to `add(int)'
  collect2.exe: error: ld returned 1 exit status
  exit 1

gcc good/main.c good/lib.c         -> exit 0; run exit 0
nm good.exe                        ->  ... T missing_func

ar rcs libmulti.a foo.o bar.o
gcc good/arch_main.c libmulti.a -o arch.exe   -> exit 0
nm arch.exe                        ->  ... T foo        (bar ABSENT)
gcc good/arch_main.c -Wl,--whole-archive libmulti.a -Wl,--no-whole-archive -o whole.exe
                                     -> exit 0
nm whole.exe                       ->  ... T bar
                                      ... T foo        (BOTH present)

gcc bad/cascade.c
  ld.exe: ...cascade.c:(.text+0xe): undefined reference to `__ksym_1'
  ld.exe: ...cascade.c:(.text+0x13): undefined reference to `__ksym_2'
  ... (12 total, addresses .text+0xe .. .text+0x45)
  collect2.exe: error: ld returned 1 exit status
  exit 1
```

## Verified facts

- Undefined references produce `undefined reference to '<name>'` + a
  `collect2: ld returned 1 exit status` summary; exit code 1. KNOWN (recorded).
- `nm` shows `U` for undefined, `T` for defined; `objdump -t` shows `sec 0`
  rows for undefined on COFF. KNOWN (recorded).
- C++ signature mismatch yields `undefined reference to 'add(int)'` while the
  library defines `_Z3addd` = `add(double)`. KNOWN (recorded).
- Archive members are pulled lazily: only `foo` appears without
  `--whole-archive`; both `foo` and `bar` appear with it. KNOWN (recorded).
- A cascade of N undefined refs is N error lines + 1 collect2 line sharing one
  prefix; the recorded mini-case is the kernel 40k-case in miniature. KNOWN.
- `readelf -s` does not work on PE/COFF objects; objdump -t is the MinGW
  equivalent. KNOWN (recorded error).

## False-positive evals (correct code must not be flagged)

- A `T add(double)` definition with an `add(int)` caller is a signature/
  mangling mismatch — must NOT be reported as "definition missing".
- An archive member that nothing references (e.g. `bar.o` in a default link) is
  normal lazy extraction — must NOT be flagged as a bug.
- `readelf -s` failing on COFF is a tool/platform mismatch, not a link error.

## Historical evals

- Kernel link with 40,784 undefined references (`__jump_table`, `__ksymtab`)
  misread as "the linker broke": replay with `bad/cascade.c` — the correct
  output is "one missing generated definition source", proven by the shared
  prefix and the single collect2 summary.
- Replay the "symbol exists in source but link fails" class with
  `mismatch_*`: the resolution is `nm -C`, never a rewrite of the caller.

## Adversarial evals

- A proposed "fix" that adds `-lfoo` or reorders archives WITHOUT checking the
  symbol table must be rejected unless `nm`/`objdump -t` proves the referenced
  symbol is now DEFINED.
- A "fix" that adds `extern "C"` to only one side of a C++ mismatch is
  insufficient; verify the mangled name matches the reference.

## Scoring (for routing eval)

- precision: every flag maps to a reference rule (1-6).
- recall: undefined refs, mangling mismatches, lazy archive pull-in,
  whole-archive forcing, cascades, and as-needed/interposition are all covered.
- FP-rate: correct definitions, lazy extraction, and tool/platform mismatches
  produce zero flags.

## Target toolchains (absent, documented)

- `lld`: not installed; semantics cross-checked against lld-docs; the symbol
  table commands are identical.
- ELF/glibc host: `readelf -s`, `nm -D`, `objdump -T`, GLIBCXX/GLIBC versioned
  symbols, `--as-needed` shared-library demos — documented as target
  verification, not executed here (COFF host).
