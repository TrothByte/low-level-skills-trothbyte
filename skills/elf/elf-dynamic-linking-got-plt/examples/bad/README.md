# examples/bad — two wrong visibility assumptions across the dynamic boundary

Each subdirectory reproduces one failure class. Both were run on this host (MinGW gcc
16.1 / binutils 2.46, PE/COFF) and the diagnostics recorded (VERIFIED); the ELF
variants are documented-as-target.

## 1. visibility-overreach — reaching file-local state from the executable

`libstate.c` keeps its state in `static int counter` (STB_LOCAL analog). `main.c`
declares `extern int counter` and reads it directly, assuming the global name in the
library source is reachable across the boundary.

VERIFIED on this host — static link fails:

```
gcc -c libstate.c -o libstate.o
gcc -c main.c -o main.o
gcc main.o libstate.o -o bad_vis_static.exe
# ld: main.o:main.c:(.rdata$.refptr.counter[.refptr.counter]+0x0):
#     undefined reference to `counter'
# collect2: error: ld returned 1 exit status           -> exit 1
```

VERIFIED — dynamic link fails the same way (a `static` symbol is never exported, so the
import library cannot satisfy it):

```
gcc -shared -o libstate.dll libstate.o -Wl,--out-implib=libstate.dll.a
gcc main.o -L. -lstate -o bad_vis_dyn.exe
# ld: ... undefined reference to `counter'             -> exit 1
```

On ELF the mechanism is identical (DOCUMENTED-AS-TARGET): `counter` is `STB_LOCAL`,
never in `.dynsym`, so both the link and (if it somehow linked) load would fail.
`nm libstate.o` shows `b counter`; `nm -D libstate.dll` shows no `counter`.

Lesson: only default-visibility globals exported via `.dynsym` (ELF) / the export table
(PE) cross the boundary; reach private state through the library's accessor functions.

## 2. interposition — expecting an executable definition to override a library call

`libmath.c` exports `helper` and `compute`, where `compute` internally calls `helper`.
`main.c` also defines `helper` and expects the library's internal call to bind to the
executable's version (interposition).

VERIFIED on this host — no interposition on PE: the DLL's internal call stays in the
DLL, so `compute(1) = 3` (library's `helper`), exit 0:

```
gcc -c libmath.c -o libmath.o
gcc -c main.c -o main.o
gcc -shared -o libmath.dll libmath.o -Wl,--out-implib=libmath.dll.a
gcc main.o -L. -lmath -o interp_dyn.exe
./interp_dyn.exe
# compute(1) = 3       -> exit 0
```

VERIFIED — statically linked, the duplicate definition is a hard error:

```
gcc main.o libmath.o -o interp_static.exe
# ld: libmath.o:libmath.c:(.text+0x0): multiple definition of `helper'
# ld: main.o:main.c:(.text+0x0): first defined here
# collect2: error: ld returned 1 exit status           -> exit 1
```

On ELF (DOCUMENTED-AS-TARGET) interposition *can* occur, but only if the executable's
`helper` is in its dynamic symbol table. Default link: not exported, no interposition,
`compute(1) = 3`. With `-rdynamic`/`--export-dynamic`: `helper` enters `.dynsym`,
the loader binds the library's internal call to the executable's version, and
`compute(1) = 101`. `LD_DEBUG=bindings` shows which definition won.

Lesson: interposition is a loader decision driven by scope order and by what is in the
executable's `.dynsym`; it is not "the executable always wins" and it does not exist on
PE, where each DLL is a separate binding scope.

## Lesson across both

Correct visibility: the executable consumes only exported symbols via the documented
API. Wrong visibility: reaching into file-local state (`undefined reference`), or
assuming a duplicate definition in the executable silently overrides library-internal
calls (interposition) when no export mechanism (`-rdynamic` on ELF) was set up.
