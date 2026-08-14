# examples/bad — four failure classes, verified and documented

Each subdirectory is a minimal reproduction of one failure class. Every command was run
on MinGW (PE/COFF, gcc 16.1, GNU ld 2.46) and the results are recorded; the ELF-side
behavior is documented as target verification (Linux/WSL).

## 1. symbol-mismatch — caller and provider disagree on the name

`main.c` calls `foo_add`; `libfoo_wrong.c` defines `foo_addition`.

```
gcc -Wall -Wextra -Werror -O2 -c libfoo_wrong.c -o libfoo_wrong.o   # exit 0
gcc -Wall -Wextra -Werror -O2 -c main.c -o main.o                   # exit 0
gcc main.o libfoo_wrong.o -o bad1.exe
# ld: main.o: (.text.startup+0x14): undefined reference to `foo_add'
# collect2: error: ld returned 1 exit status          -> exit 1
```

Diagnosis: `nm main.o` shows `U foo_add`; `nm libfoo_wrong.o` shows `T foo_addition`.
Neither compilation failed; the LINKER could not resolve the `U`.

## 2. multiple-definition — two strong definitions of one symbol

`a.c` and `b.c` both define `foo_add` with different bodies.

```
gcc -Wall -Wextra -Werror -O2 a.c b.c -o bad2.exe
# ld: b.c:(.text+0x0): multiple definition of `foo_add'
# ld: a.c:(.text+0x0): first defined here
# collect2: error: ld returned 1 exit status          -> exit 1
```

Diagnosis: two `T foo_add` entries in the link. Remove or rename one definition.

## 3. unresolved-at-load — a symbol with no provider at all

`libbar.c` calls `qux_missing`; `main.c` calls `bar_caller`.

```
gcc -shared -o libbar.dll libbar.c
# ld: libbar.c:(.text+0xe): undefined reference to `qux_missing'   -> exit 1
```

VERIFIED on this MinGW toolchain: GNU ld 2.46 rejects an undefined symbol inside a
`-shared` DLL at LINK time (also with `-Wl,--allow-shlib-undefined`; the option only
relaxes undefined symbols coming from other shared libraries, not from the objects being
linked).

DOCUMENTED-AS-TARGET (ELF): the same source links successfully twice — `gcc -shared -o
libbar.so libbar.c` and `gcc main.c -L. -lbar -o prog` both exit 0 — and the failure is
deferred to the dynamic loader:
- undefined *data* symbol → load fails immediately (GOT relocation applied eagerly);
- undefined *function* under lazy binding → fails at first call, not at startup;
- `LD_DEBUG=symbols LD_BIND_NOW ./prog` shows the resolution failing at load.

This is the "link vs load" distinction: which tool fails depends on the platform's
linking model, not on the C source.

## 4. missing-fpic — shared object built without position independence

`libfoo_nopic.c` takes the address of a global (`int *foo_addr(void) { return
&foo_global; }`), which on x86-64 ELF produces a 32-bit absolute relocation when built
without `-fPIC`.

On MinGW/PE this is NOT an error (the loader applies base relocations; PIC is not
required for DLLs) — VERIFIED:

```
gcc -fno-pic -shared -o libfoo_nopic.dll libfoo_nopic.c     # exit 0
gcc -Wall -Wextra -Werror -O2 -c libfoo_nopic.c -o libfoo_nopic.o   # exit 0
gcc -Wall -Wextra -Werror -O2 main.c libfoo_nopic.o -o bad4.exe     # exit 0
./bad4.exe                                                          # exit 0
```

DOCUMENTED-AS-TARGET (ELF, x86-64):

```
gcc -fno-pic -shared -o libfoo.so libfoo_nopic.c
# ld: relocation R_X86_64_32S against symbol `foo_global' can not be used
#     when making a shared object; recompile with -fPIC   -> exit 1
gcc -fPIC -shared -o libfoo.so libfoo_nopic.c                        # exit 0
```

Lesson: `-fPIC` is a correctness requirement for x86-64 ELF shared objects, not a
Windows-style "extra option".
