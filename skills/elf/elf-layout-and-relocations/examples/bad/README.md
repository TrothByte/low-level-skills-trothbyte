# examples/bad — three failure classes reproduced and recorded

Each subdirectory is a minimal reproduction of one failure class. The multiple-definition
and undefined-symbol cases were run on this host (MinGW gcc 16.1 / binutils 2.46,
PE/COFF) and the diagnostics recorded (VERIFIED). The missing-`-fPIC` case is a
DOCUMENTED-AS-TARGET x86-64 ELF error; on this MinGW host the same source builds and
runs, which is itself a recorded fact.

## 1. missing-fpic — shared object without position independence

`libnopic.c` takes the address of a global (`&shared_counter`). On x86-64 ELF this
compiles to a 32-bit absolute relocation (`R_X86_64_32S`) without `-fPIC`, which the
linker rejects when making a shared object.

DOCUMENTED-AS-TARGET (ELF host required; not executed on this host):

```
gcc -fno-pic -shared -o libnopic.so libnopic.c
# ld: relocation R_X86_64_32S against symbol `shared_counter' can not be used
#     when making a shared object; recompile with -fPIC     -> exit 1
gcc -fPIC -shared -o libnopic.so libnopic.c                  # exit 0
readelf -r libnopic.so    # now R_X86_64_GOTPCREL / R_X86_64_REX_GOTPCRELX, no 32S
```

VERIFIED on this MinGW host — the identical source needs no PIC on PE/COFF:

```
gcc -fno-pic -shared -o libnopic.dll libnopic.c     # exit 0
gcc -Wall -Wextra -Werror -O2 -c libnopic.c -o libnopic.o   # exit 0
gcc -Wall -Wextra -Werror -O2 -c main.c -o main.o           # exit 0
gcc main.o libnopic.o -o bad_fpic.exe                       # exit 0
./bad_fpic.exe                                              # exit 0
```

Lesson: on x86-64 ELF, `-fPIC` is a correctness requirement for shared objects, not a
performance tweak, and NOT a Windows convention.

## 2. symbol-conflict — two strong definitions of one symbol

`a.c` and `b.c` both define `helper` with different bodies.

```
gcc -Wall -Wextra -Werror -O2 a.c b.c -o bad_dup.exe
# ld: b.c:(.text+0x0): multiple definition of `helper'
# ld: a.c:(.text+0x0): first defined here
# collect2: error: ld returned 1 exit status          -> exit 1
```

VERIFIED. Diagnosis: `nm a.o` and `nm b.o` both show `T helper`; the linker sees two
strong global definitions and refuses. Rename or `static`-ize one.

## 3. undefined-symbol — a reference with no provider

`main.c` calls `never_defined_here`, which is declared but never defined anywhere.

```
gcc -Wall -Wextra -Werror -O2 main.c -o bad_undef.exe
# ld: ...: undefined reference to `never_defined_here'
# collect2: error: ld returned 1 exit status          -> exit 1
```

VERIFIED. Diagnosis: `nm main.o` shows `U never_defined_here`; no object on the link
line provides a `T`/`D` definition. The compile step is clean; the LINKER fails.

## Lesson across all three

Classify the failure by the tool that emits it: missing `-fPIC` is a linker rejection of
a compile-time decision (fix: recompile with `-fPIC` on ELF); `multiple definition` and
`undefined reference` are symbol-table mismatches visible with `nm`.
