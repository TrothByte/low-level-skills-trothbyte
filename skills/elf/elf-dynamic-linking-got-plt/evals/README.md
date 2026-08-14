# Evaluation — elf-dynamic-linking-got-plt

Skill: `skills/elf/elf-dynamic-linking-got-plt`. Stability target: `evaluated`.

## Synthetic evals

- **easy/positive**: given `objdump -d` output with `jmp *rel32(%rip)  # __imp_foo`
  (PE) or `call *rel(%rip) @plt` (ELF), name the mechanism (PLT stub / import thunk
  through a GOT/IAT slot) and the table it indirects through.
- **easy/negative**: `.got` vs `.got.plt` — given `readelf -d` (`PLTGOT`, `JMPREL`,
  `RELA`), say which array holds lazily-bound function slots and which holds data
  relocations; a claim that GOT[0] holds "the first global's address" must be corrected
  (reserved resolver entries).
- **medium/positive**: given `readelf -d` + `objdump -R` excerpts, list the dynamic
  entries (`NEEDED`, `JMPREL`, `RELASZ`, `FLAGS`/`FLAGS_1` NOW) and the relocation
  classes (`RELATIVE`, `GLOB_DAT`, `JUMP_SLOT`) with their roles; decide lazy vs eager.
- **medium/negative**: predict failure phase — a missing *function* fails at first call
  under lazy binding but at load under `LD_BIND_NOW`; a missing *data* symbol fails at
  load in both modes.
- **hard/negative**: compute `R_X86_64_PLT32 = L + A - P` and
  `R_X86_64_GOTPCREL = G + GOT + A - P` for concrete values; explain when the linker
  relaxes `PLT32` to `PC32`.
- **adversarial**: exe (linked with and without `-rdynamic`) and `.so` both define
  `helper`; the `.so`'s `compute` calls `helper` — predict which definition wins in each
  case and justify with `.dynsym` scope rules.

## False-positive evals (correct code must NOT be flagged)

- Default lazy binding is correct; do NOT demand `LD_BIND_NOW`/`-z now` as "hardening".
- `PLT32`/`GOTPCREL` in PIC code and `PC32` for non-preemptible internal calls are all
  correct; do NOT flag them as inefficiency or "text relocations".
- A GOT slot pointing at the resolver before the first call is the *design*, not a bug.
- `examples/good` must build clean (`-Wall -Wextra -Werror -O2`), link, run with exit 0,
  and export exactly `counter_bump`/`counter_get`/`counter_reset`.
- MinGW/PE builds are correct without `-fPIC` and without any lazy-binding model; do NOT
  demand ELF semantics (`LD_BIND_NOW`, interposition) on PE.

## Verification commands

On this host (MinGW, PE/COFF):

```
gcc -Wall -Wextra -Werror -O2 -DLIBCOUNTER_BUILD -c examples/good/libcounter.c -o libcounter.o
gcc -Wall -Wextra -Werror -O2 -c examples/good/main.c -o main.o
gcc -shared -o libcounter.dll libcounter.o -Wl,--out-implib=libcounter.dll.a
gcc main.o -L. -lcounter -o good_dyn.exe && ./good_dyn.exe       # counter = 3, exit 0
objdump -p libcounter.dll    # export table: counter_bump/counter_get/counter_reset
objdump -p good_dyn.exe      # DLL Name: libcounter.dll imports
objdump -d good_dyn.exe      # jmp *rel(%rip) # __imp_counter_* thunks
objdump -R good_dyn.exe      # "not a dynamic object" -> exit 1
gcc main.o libcounter.o -o good_static.exe && ./good_static.exe  # exit 0
gcc -c examples/bad/visibility-overreach/libstate.c -o libstate.o
gcc -c examples/bad/visibility-overreach/main.c -o vmain.o
gcc vmain.o libstate.o -o b1.exe          # undefined reference to 'counter' -> exit 1
gcc vmain.o -L. -lstate -o b2.exe         # undefined reference to 'counter' -> exit 1
gcc -c examples/bad/interposition/libmath.c -o libmath.o
gcc -c examples/bad/interposition/main.c -o imain.o
gcc -shared -o libmath.dll libmath.o -Wl,--out-implib=libmath.dll.a
gcc imain.o -L. -lmath -o b3.exe && ./b3.exe   # compute(1) = 3, exit 0 (no PE interposition)
gcc imain.o libmath.o -o b4.exe           # multiple definition of 'helper' -> exit 1
```

On an ELF host (Linux or WSL; requires readelf):

```
gcc -fPIC -shared -o libcounter.so examples/good/libcounter.c
gcc -pie -fPIE examples/good/main.c -L. -lcounter -Wl,-rpath,'$ORIGIN' -o prog
readelf -d prog          # NEEDED libcounter.so, JMPREL, RELA, no NOW
objdump -R prog          # R_X86_64_JUMP_SLOT x3, RELATIVE
objdump -d prog | grep '@plt'
nm -D libcounter.so      # counter_bump counter_get counter_reset
LD_DEBUG=bindings ./prog    # binding on first call only
LD_BIND_NOW=1 ./prog        # all bindings at load
gcc -z now -o prog_now main.o -L. -lcounter -Wl,-rpath,'$ORIGIN'
readelf -d prog_now      # FLAGS_1: NOW
gcc -rdynamic -o prog_rdyn main.o -L. -lcounter -Wl,-rpath,'$ORIGIN'
nm -D prog_rdyn | grep helper    # helper now in the exe's .dynsym -> interposes
```

## Verified facts (this repository's host: MinGW gcc 16.1, binutils 2.46, PE/COFF)

| Fact | Result | How verified |
|---|---|---|
| `libcounter.c`/`main.c` compile with `-Wall -Wextra -Werror -O2` | exit 0 | gcc |
| DLL dynamic link + run | `counter = 3`, exit 0 | gcc + run |
| DLL exports exactly `counter_bump`/`counter_get`/`counter_reset` | export table size 3, no `counter` | objdump -p |
| Exe imports `libcounter.dll` | `DLL Name: libcounter.dll` | objdump -p |
| Import thunks (PLT analog) | `jmp *0x6e0a(%rip)  # __imp_counter_reset` | objdump -d |
| Main calls indirect through IAT | `call *0x5860(%rip) # __imp_counter_reset`; `call *%rbx` via `__IAT_start__` | objdump -d |
| `objdump -R` on PE exe | `not a dynamic object`, exit 1 | objdump |
| Static link + run of good case | exit 0, `counter = 3` | gcc + run |
| File-local `counter` reached from exe (static link) | `undefined reference to 'counter'`, exit 1 | gcc link |
| Same, dynamic link against import lib | `undefined reference to 'counter'`, exit 1 | gcc link |
| Duplicate `helper` (static link) | `multiple definition of 'helper'`, exit 1 | gcc link |
| No PE interposition: DLL internal `helper` stays in DLL | `compute(1) = 3`, exit 0 | gcc + run |

## Documented-as-target facts (ELF host required, NOT executed on this host)

| Claim | Expected result |
|---|---|
| GOT/PLT layout (`.got`, `.got.plt` 3 reserved entries, `.plt` stubs) | `readelf -S`/`-d`, `objdump -d` |
| Lazy binding: first call → PLT stub → `_dl_runtime_resolve` → GOT patched | gdb + `LD_DEBUG=bindings` |
| `R_X86_64_PLT32 = L+A-P`, `R_X86_64_GOTPCREL = G+GOT+A-P` | `readelf -r` + computation |
| `LD_BIND_NOW` / `-z now` → all bindings at load, missing function fails at load | `LD_BIND_NOW=1 ./prog`, `readelf -d` `FLAGS_1: NOW` |
| `objdump -R` → `R_X86_64_RELATIVE`/`GLOB_DAT`/`JUMP_SLOT` | `objdump -R prog` |
| `readelf -d` `NEEDED`/`PLTGOT`/`JMPREL`/`RELA`/`FLAGS` | `readelf -d prog` |
| Interposition requires exe symbol in `.dynsym` (`-rdynamic`) | `nm -D` + `LD_DEBUG=bindings` |
| `-fPIC` vs `-fPIE` codegen difference (preemption assumption) | `readelf -r` on both builds |

## Scoring (for routing eval)

- precision: every `readelf`/`objdump`/binding claim must match the cited rule; no
  PE-vs-ELF conflation.
- recall: each wrong assumption (file-local reach-over, unconditional interposition,
  "always fails at load") must be detected.
- FP-rate: `examples/good`, default lazy binding, PIC/PIE code, and PE builds produce
  zero findings.
- platform correctness: ELF-only claims (lazy binding, `objdump -R`, `LD_BIND_NOW`,
  interposition) are labeled documented-as-target; PE facts are labeled VERIFIED.
