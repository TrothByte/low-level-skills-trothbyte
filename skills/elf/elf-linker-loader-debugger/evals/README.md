# Evaluation — elf-linker-loader-debugger

Skill: `skills/elf/elf-linker-loader-debugger`. Stability target: `evaluated`.

## Synthetic evals

- **easy/positive**: correct extern linkage — `examples/good` must build and run
  (static and shared), and the agent must NOT invent an error.
- **easy/negative**: `examples/bad/symbol-mismatch` — must identify the failure as a
  link-time `undefined reference` to `foo_add` and pinpoint the name mismatch via `nm`.
- **medium/negative**: `examples/bad/multiple-definition` — must report
  `multiple definition of 'foo_add'` and locate both providers.
- **medium/negative**: `examples/bad/missing-fpic` — on an ELF target the agent must
  predict the `R_X86_64_32S ... recompile with -fPIC` error for `gcc -fno-pic -shared`,
  and on MinGW must correctly state that no `-fPIC` is needed.
- **hard/negative**: `examples/bad/unresolved-at-load` — must classify the failure by
  phase and platform: MinGW → link-time error (VERIFIED); ELF → link succeeds, loader
  fails (data at load, function at first call under lazy binding).
- **adversarial**: given `readelf -r` output showing `R_X86_64_JUMP_SLOT` entries, predict
  when an undefined function's first call resolves and when it fails; given
  `__attribute__((constructor))` in two objects linked in a defined order, state which
  init functions run before `main` and in what order relative to each other.

## False-positive evals (correct code must NOT be flagged)

- A non-`static` library function that `main` never calls directly (e.g. `foo_get_calls`)
  is a legitimate export; do NOT flag it as "unused symbol error".
- A MinGW DLL build without `-fPIC` is correct; do NOT require `-fPIC` on PE/COFF.
- A clean static link (`gcc main.o libfoo.o -o prog`) must not be reported as "dynamic
  loader risk".
- `.init_array` ordering: code that relies only on per-object order must not be flagged
  as wrong when the agent correctly states that cross-object order is unspecified.

## Verification commands

On the current host (MinGW):

```
gcc -Wall -Wextra -Werror -O2 -c libfoo.c -o libfoo.o
gcc -Wall -Wextra -Werror -O2 -c main.c -o main.o
gcc main.o libfoo.o -o good_static.exe && ./good_static.exe      # expect 0
gcc -shared -o libfoo.dll libfoo.o -Wl,--out-implib,libfoo.dll.a
gcc main.o -L. -lfoo -o good_dyn.exe && ./good_dyn.exe           # expect 0
nm main.o && nm libfoo.o                                          # U/T/T/b
objdump -r main.o                                                 # REL32 foo_add
objdump -p good_dyn.exe                                           # DLL Name: libfoo.dll
gcc -shared -o libbar.dll libbar.c                                # expect exit 1
gcc a.c b.c -o bad2.exe                                           # expect exit 1
```

On an ELF host (Linux or WSL; requires readelf — readelf rejects PE/COFF):

```
gcc -fPIC -shared -o libfoo.so libfoo.c
gcc main.c -L. -lfoo -Wl,-rpath,'$ORIGIN' -o prog && LD_LIBRARY_PATH=. ./prog
readelf -h -S -l -s -d -r prog
objdump -R prog
gcc -fno-pic -shared -o libfoo_nopic.so libfoo_nopic.c            # expect R_X86_64_32S error
LD_DEBUG=symbols LD_BIND_NOW ./prog
gdb -batch -ex "break main" -ex run -ex "info line main" -ex bt prog
```

## Verified facts (this repository's host: MinGW gcc 16.1, binutils 2.46)

| Fact | Result | How verified |
|---|---|---|
| Clean compile with `-Wall -Wextra -Werror -O2` | exit 0, good + bad sources | gcc |
| Static link + run of good case | exit 0, output `foo_add(2, 3) = 5`, `calls = 1` | gcc + run |
| `nm main.o` shows `U foo_add` / `T main` | confirmed | nm |
| `nm libfoo.o` shows `T foo_add`, `b calls` | confirmed | nm |
| Object call placeholder + relocation record | `call 9 <main+9>` + `IMAGE_REL_AMD64_REL32 foo_add` | objdump -d/-r |
| Linker patches the call displacement | `call 140001490 <foo_add>` | objdump -d |
| Shared build + dynamic link + run | exit 0 (three times) | gcc + run |
| Executable imports from libfoo.dll | `DLL Name: libfoo.dll`, foo_add/foo_get_calls | objdump -p |
| DLL exports foo_add/foo_get_calls only | export table size 2 | objdump -p |
| Symbol mismatch → `undefined reference to 'foo_add'` | exit 1 | gcc link |
| Multiple definition → `multiple definition of 'foo_add'` | exit 1 | gcc link |
| Undefined symbol inside `-shared` DLL | exit 1 at LINK (even with `--allow-shlib-undefined`) | gcc link |
| `-fno-pic -shared` DLL on MinGW | exit 0, runs, exit 0 | gcc + run |
| gdb maps main.c line 5 to 0x140001490, steps into foo_add | break/step/bt all correct | gdb batch |
| gdb at -O2 shows entry values, optimized-out locals | `a=a@entry=2`, `No locals.` | gdb batch |
| readelf on PE/COFF | `Error: Not an ELF file - wrong magic bytes` | readelf |

## Documented-as-target facts (ELF host required, NOT executed on this host)

| Claim | Expected result |
|---|---|
| `readelf -h/-S/-l/-s/-d/-r` on an ELF binary | ELF header, sections, segments, symbols, dynamic, relocations |
| `objdump -R` | `R_X86_64_JUMP_SLOT`/import relocations for external calls |
| Lazy binding | undefined *function* fails at first call, undefined *data* at load |
| `-z now` / `LD_BIND_NOW` | all bindings resolved at load |
| `-fno-pic -shared` on x86-64 ELF with global-address code | `relocation R_X86_64_32S ... recompile with -fPIC` |
| `_start` → `__libc_start_main` → `.preinit_array`/`.init_array` → `main` | observable in gdb at `_start` vs `main` |
| `.init_array` runs before `main` | `readelf -d` INIT_ARRAY + gdb breakpoints |

## Scoring (for routing eval)

- precision: every claimed error must map to the correct pipeline stage (compile/link/load/run).
- recall: each bad case must be detected.
- FP-rate: `examples/good` and the false-positive list must produce zero findings.
- platform correctness: ELF-only claims (PIC, loader semantics, readelf) must be
  stated as requiring an ELF host, never as "verified on Windows".
