# Evaluation — core-dump-analysis

Skill: `skills/debugging/core-dump-analysis`.
Stability target: `source-backed` for the extract-bt/registers/maps workflow,
`researched` for core-file-specific steps (see host limitations below).

## Verified facts (host, recorded 2026-08-20)

Toolchain: Windows x64, gcc 16.1.0 (MSYS2 MinGW, x86_64-w64-mingw32, UCRT),
gdb 17.2 (`C:\msys64\ucrt64\bin\gdb.exe`), python 3.11.9, bash (MSYS2).

Fixtures compile and crash deterministically:

```
gcc -g -O0 -fno-stack-protector examples/bad/crash.c -o crash.exe
gcc -g -O0 examples/good/clean_app.c -o clean_app.exe
```

`examples/bad/crash.c` overflow is a stack-buffer overflow that clobbers the
saved return address with 'A' fill; the crash is a SIGSEGV in `ucrtbase!strlen`
reading the clobbered pointer. Extracted with gdb (full output in
`evals/recorded/gdb_live.txt`):

```
Thread 1 received signal SIGSEGV, Segmentation fault.
0x00007fff2608d900 in ucrtbase!strlen () from C:\WINDOWS\System32\ucrtbase.dll
#0  0x00007fff2608d900 in ucrtbase!strlen () from C:\WINDOWS\System32\ucrtbase.dll
#1  0x00007ff67a2d14b8 in crash_here (user_input=0x4141414141414141 <error: Cannot access memory at address 0x4141414141414141>) at skills/debugging/core-dump-analysis/examples/bad/crash.c:27
#2  0x4141414141414141 in ?? ()
#3  0x4141414141414141 in ?? ()
#4  0x4141414141414141 in ?? ()
#5  0x4141414141414141 in ?? ()
#6  0x4141414141414141 in ?? ()
#7  0x4141414141414141 in ?? ()
...
rip            0x7fff2608d900      0x7fff2608d900 <ucrtbase!strlen+16>
rsp            0x5ffe08            0x5ffe08
rbp            0x5ffe40            0x5ffe40
rax            0x4141414141414141  4702111234474983745
```

Stack memory around the fault site (`x/24wx $rsp-0x40`, tail):

```
0x5ffdc8:	0x260d98c0	0x00007fff	0x00000001	0x00000000
0x5ffdd8:	0x25fa983e	0x00007fff	0x005ffe50	0x00000000
0x5ffde8:	0x25fa9315	0x00007fff	0x00753710	0x00000000
0x5ffdf8:	0x00000000	0x00000000	0x00000000	0x00000000
0x5ffe08:	0x7a2d14b8	0x00007ff6	0x00000001	0x00000000
0x5ffe18:	0x25fa983e	0x00007fff	0x005ffe90	0x00000000
```

Analyzer on the recorded gdb output (`python examples/tools/core_analyze.py
evals/recorded/gdb_live.txt`), REAL output:

```
=== core-dump-analysis summary ===
signal      : SIGSEGV, (thread 1)
rip         : 0x00007fff2608d900
fault @0x00007fff2608d900  -> .text in C:\WINDOWS\System32\ucrtbase.dll
fault @0x4141414141414141  -> NOT IN ANY MAPPED REGION
faulting    : frame 0 0x00007fff2608d900 in ucrtbase!strlen() from C:\WINDOWS\System32\ucrtbase.dll
STACK CORRUPTION: 7 of 11 return addresses outside mapped regions:
  frame 2: 0x4141414141414141 unmapped
  frame 3: 0x4141414141414141 unmapped
  frame 4: 0x4141414141414141 unmapped
  frame 5: 0x4141414141414141 unmapped
  frame 6: 0x4141414141414141 unmapped
  frame 7: 0x4141414141414141 unmapped
  pattern 0x4141414141414141 ('A' fill) -> buffer overflow / overwritten return address
  note: 1 frames mapped but unsymbolized (not necessarily corrupt)
rsp         : 0x00000000005ffe08 -> NOT IN ANY MAPPED REGION
rbp         : 0x00000000005ffe40 -> NOT IN ANY MAPPED REGION
verdict     : 12 frames, signal SIGSEGV,
```

`examples/tools/run_postmortem.sh` reproduces the same verdict end-to-end
(REAL output, `evals/recorded/postmortem.txt`):

```
signal      : SIGSEGV, (thread 1)
...
faulting    : frame 0 0x00007fff2608d900 in ucrtbase!strlen() from C:\WINDOWS\System32\ucrtbase.dll
STACK CORRUPTION: 7 of 11 return addresses outside mapped regions:
...
  pattern 0x4141414141414141 ('A' fill) -> buffer overflow / overwritten return address
verdict     : 12 frames, signal SIGSEGV,
```

Clean control (`examples/good/clean_app.c`, REAL output in
`evals/recorded/gdb_clean.txt`):

```
clean_app: sum=-5, expected=-5
clean_app: ok, exiting 0
[Inferior 1 (process 21472) exited normally]
```
```
=== core-dump-analysis summary ===
signal      : (none detected - no crash evidence)
rip         : (missing)
faulting    : (no backtrace frames parsed)
stack       : (no frames to check)
verdict     : no crash evidence - clean run (no fault)
```

Host limitations (measured, so recorded):

```
gdb -batch -ex "set pagination off" -ex run -ex "generate-core-file crash.core" --args crash.exe
  gdb.exe : warning: cannot close "<temp>\crash.core": invalid operation
  Can't create a corefile
```
```
gdb -batch -ex "info proc mappings" --args crash.exe
  Not supported on this target.
```

On Windows MinGW gdb, `generate-core-file` and `info proc mappings` are
UNSUPPORTED; the workflow substitutes the live crash stop and `info files`
module sections (which map every address of interest to a module). Loading a
core file and `info proc mappings` are verified only as the target/Linux
workflow (gdb on Linux, coredumpctl, WinDbg). Markers: host steps KNOWN,
core-file-loading steps UNVERIFIED-on-host.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| crash reproduction | `bad/crash.c` | deterministic SIGSEGV under gdb | SIGSEGV in `ucrtbase!strlen`, frames 2-7 `0x4141414141414141` |
| faulting thread | `bad/crash.c` | faulting thread identified from signal line | thread 1 (`Thread 1 received signal SIGSEGV`) |
| fault address → region | `bad/crash.c` | clobbered pointer classified unmapped | `fault @0x4141414141414141 -> NOT IN ANY MAPPED REGION` |
| stack-corruption detection | `bad/crash.c` | garbage bt not trusted; 'A'-fill named | `STACK CORRUPTION: 7 of 11 ... pattern 'A' fill` |
| register cross-check | `bad/crash.c` | rip symbolizable, rax holds poisoned pointer | `rip ... <ucrtbase!strlen+16>`, `rax 0x4141414141414141` |
| clean run | `good/clean_app.c` | no crash, exit 0, correct sum | `sum=-5`, exited normally, `no crash evidence` |
| end-to-end script | `run_postmortem.sh` | same verdict from the scripted pipeline | identical summary via `evals/recorded/postmortem.txt` |

## False-positive evals (correct code must not be flagged)

- `good/clean_app.c` exits 0 with `sum=-5` and must produce "no crash
  evidence - clean run (no fault)" — recorded above.
- A mapped-but-unsymbolized frame (`??`) is reported as a note, never as
  corruption; corruption requires an address outside all mapped regions.
- `info files` section listings on a healthy process (module maps) must not
  be misread as crash evidence on their own.

## Historical evals (famous post-mortems map to the rules)

- Heartbleed-era memory analysis (CVE-2014-0160): reading process memory
  across a 64 KB boundary from a heartbeat request — the same
  registers-and-memory-extraction loop, applied to evidence capture before
  the crash. Reference rules 3 (fault/memory region) and 5 (extract evidence
  with concrete commands).
- Spectre-era crash post-mortems (2018): faulting address in a region that
  the instruction stream never legitimately touches, plus speculation-induced
  garbage reads — region classification (rule 3) and not trusting where the
  instruction "should" have been (rule 4).
- General production post-mortems: a misleading top frame (crash site looks
  innocent) is resolved only by validating the frames against maps and
  registers — exactly the `crash.c` fixture pattern.

## Adversarial evals

- `bad/crash.c` IS the adversarial case: the backtrace is garbage by design.
  The agent must NOT edit `crash_here`'s `strcpy` replacement based on bt
  alone; the correct conclusion is "return address overwritten by a stack
  overflow", evidenced by unmapped return addresses, the 'A' fill, and rax.
- Variant: overflow a different local (e.g. a 16-byte buffer with an even
  longer copy) — the unmapped-return-address check must still fire, and the
  fault address must still be classified unmapped.
- Variant: heap out-of-bounds write that clobbers a function pointer, then a
  call through the poisoned pointer — frames may be intact but the call
  target is unmapped; the analyzer must flag the unmapped target even though
  the stack itself is not 'A'-filled.
- Multi-threaded variant: faulting thread is a worker, not main — `thread
  apply all bt` + signal line must select the right thread (the Windhawk
  threads in the recorded environment exercise exactly this).

## Verification commands (target — Linux coredumpctl, WinDbg)

Linux core-file workflow (target; not host-verifiable on Windows gdb):

```
ulimit -c unlimited
cat /proc/sys/kernel/core_pattern
gdb -batch -ex "set pagination off" -ex "thread apply all bt" \
    -ex "info registers" -ex "info proc mappings" ./app core
coredumpctl list
coredumpctl gdb <pid>
eu-readelf -n ./app     # build id for provenance check
```

Windows workflow:

```
procdump -ma -e app.exe app.dmp
cdb -z app.dmp -c "~* kp; .exr -1; !analyze -v; q"
WinDbg: .dump /ma app.dmp   # full minidump for later analysis
```

## Scoring

- Level 1 (facts): extract signal, faulting thread, rip, and fault address
  from a core; classify the fault address into a region.
- Level 2 (correlation): correlate registers + maps + symbols; produce a
  root-cause hypothesis for the WRITER of the corrupted state.
- Level 3 (corrupted stack): given a garbage backtrace, fall back to
  registers and maps, name the corruption pattern, and locate the overflowed
  buffer in `x/` output.
- Level 4 (provenance): reject a core whose binary does not match (build id /
  path / timestamp) instead of analyzing it.
- Pass mark: Level 2 on `bad/crash.c` with no false positives on
  `good/clean_app.c`.
