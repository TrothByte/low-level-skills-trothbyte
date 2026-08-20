---
name: core-dump-analysis
description: Use when a process crashed and a core dump or coredump is available — extracting backtraces, register state, memory maps, heap clues, or analyzing multi-threaded crashes. Goes beyond crash triage to full post-mortem analysis of core files.
---

# Core Dump Post-Mortem Analysis

Post-mortem analysis works on the frozen state of a crashed process — registers,
memory, threads, and the signal — after the crash, complementing
`debugging-crash-triage-discipline` which governs the moment of the crash.

## When to use

- A core dump / coredump exists (from `core_pattern`, `coredumpctl`, gdb
  `generate-core-file`, WinDbg `.dump /ma`, or ProcDump) and the crash site is
  already reproduced or captured.
- Multi-threaded crashes: the faulting thread is not the one you were looking
  at, and the core lets you walk every thread's stack.
- A backtrace that looks wrong: garbage frames, "??", addresses outside any
  mapping. A core lets you validate it against registers and memory maps.
- Corrupted stacks, heap corruption, use-after-free, or overflows where the
  crash site does not name the buggy write.
- Register-level questions: what instruction faulted, what address was being
  accessed (si_addr), and which memory region that address lives in (ASLR
  makes manual guessing useless).

## When not to use

- The crash is still reproducible and no core was captured: capture the crash
  exactly with `debugging-crash-triage-discipline` first. A core is evidence
  you keep; reproduction is evidence you can repeat.
- No matching binary with symbols: a core loaded against a different build
  produces meaningless addresses. Obtain the exact build first.
- Logic errors with no fault (wrong value, no signal, no dump).
- Managed-runtime dumps (JVM hs_err, .NET dump, Go) — different tooling and
  formats; this skill targets native cores read by gdb and WinDbg/cdb.
- Performance analysis, deadlocks without a crash, or a kernel panic (that is
  `kernel-debugging-ftrace-kprobes-kdump` territory).

## What the agent often gets wrong

- Trusting a backtrace on a corrupted stack. Misleading frames are the norm
  once the return address is clobbered; the frames must be validated against
  register state and memory maps, not accepted at face value.
- Stopping at the first thread's backtrace and missing the faulting thread.
  On a core, `thread apply all bt` must be the reflex, and the signal line
  names the faulting thread.
- Not using the faulting address. The signal's si_addr (or the register that
  was being dereferenced) tells you which memory region the access targeted;
  skipping it leaves region classification to guesswork.
- Reading memory at a hand-computed address without ASLR. ASLR relocates the
  binary every run; always derive addresses from `info proc mappings` /
  `info files`, never from the load address of a previous run.
- Loading the core against a mismatched binary or one without symbols. The
  core is useless with the wrong build; check build id / path / timestamp.
- Concluding "heap corruption" with no evidence: no heap-region check, no
  chunk walk, no maps analysis, no x/ inspection. Corrupt frames are proof of
  stack corruption, not automatically of heap corruption.
- Forgetting the evidence set: siginfo + registers + memory maps + symbols
  form one correlated picture. Using registers without maps, or the backtrace
  without the fault address, produces confident wrong conclusions.
- Reasoning by averages and vibes instead of extracting evidence with gdb
  commands. Every claim in a post-mortem should trace to a command's output.

## How to reason correctly

1. Establish provenance first: is this binary the one that produced the core?
   Compare build id (`eu-readelf -n`, gdb `info files`), path, and timestamp.
   If they mismatch, stop and obtain the correct build.
2. Identify the faulting thread and the faulting instruction: read rip/pc of
   the thread named by the signal, then the fault address from si_addr (Linux)
   or from the register being dereferenced.
3. Map the fault address to a memory region with `info proc mappings` (Linux)
   or `info files` (Windows gdb): heap, stack, a loaded library, the main
   binary, or nowhere (unmapped — the access itself was the fault).
4. Validate the backtrace: every return address should fall inside a mapped
   executable region. If frames point outside all mappings (e.g. repeated
   `0x4141414141414141`), the stack is corrupted — fall back to the frame
   pointer, registers, and alternate stacks, and treat frame contents as
   payload, not call history.
5. Extract evidence with concrete commands and record the output: `bt full`,
   `thread apply all bt`, `info registers`, `info files`, `x/Nwx $rsp-0x80`.
   Never fill gaps with guesswork.
6. Correlate registers + memory + symbols into a root-cause hypothesis, then
   verify against the source. The faulting instruction is a symptom; the
   writer that produced the corrupted state is the cause.

## What to verify

- Binary matches the core: build id / path / timestamp recorded.
- Faulting thread identified, and the fault address mapped to a region (or
  proven unmapped).
- Backtrace frames are consistent with memory maps: addresses fall in
  executable ranges, or corruption is explicitly diagnosed.
- Register state sane: rsp within a stack region, rip symbolizable or its
  unmapped-ness explained.
- Memory inspection of the fault site is recorded (`x/` output) and the
  corruption pattern (e.g. 'A' fill 0x41) is named.
- The root-cause hypothesis survives source inspection of the writing code.

## How to verify

gdb is present on this host (gdb 17.2, MinGW). Compile the crashing fixture
and capture the crash stop:

```
gcc -g -O0 -fno-stack-protector examples/bad/crash.c -o crash.exe
gdb -batch -ex "set pagination off" -ex run -ex bt -ex "info registers rip rsp rbp rax" -ex "info files" -ex 'x/24wx $rsp-0x40' --args ./crash.exe
```

Then summarize the recorded output:

```
python examples/tools/core_analyze.py evals/recorded/gdb_live.txt
python examples/tools/run_postmortem.sh ./crash.exe out.txt
```

Two host limitations are recorded honestly in `evals/README.md` (measured
2026-08-20): on Windows MinGW gdb, `generate-core-file` fails ("Can't create
a corefile") and `info proc mappings` is "Not supported on this target" — the
equivalent memory map comes from `info files`, and the live crash stop is
used as post-mortem evidence. On Linux targets, run the core-file workflow:
```
ulimit -c unlimited                      # core(5): enable kernel cores
cat /proc/sys/kernel/core_pattern        # where cores land
gdb -batch -ex "thread apply all bt" ./app core
coredumpctl gdb <pid>                    # systemd-coredump cores
```

Windows target workflow:

```
procdump -ma -e app.exe app.dmp; cdb -z app.dmp -c "~* kp; r; q"
```

## Where the knowledge comes from

- GDB manual — core dump files (https://sourceware.org/gdb/current/onlinedocs/gdb.html/Core-Files.html)
- Linux kernel coredump docs (https://docs.kernel.org/admin-guide/bug-hunting.html, core(5) man page)
- Debugging with GDB — registers, memory (https://sourceware.org/gdb/current/onlinedocs/gdb.html)
- WinDbg/cdb and ProcDump for Windows crash analysis (https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/)

Host facts (gdb 17.2, gcc 16.1.0 MinGW, gdb `generate-core-file` and `info
proc mappings` unsupported on Windows) are KNOWN: measured on 2026-08-20, see
`evals/README.md`.

## Related skills

- `debugging-crash-triage-discipline` — the moment-of-crash discipline that
  produces the reproducible crash this skill analyzes post-mortem.
- `dwarf-debug-info` — how debug info makes frames and variables resolvable;
  a stripped core loses this skill's symbolization.
- `elf-linker-loader-debugger` — ASLR, module layout, and section mappings
  behind `info files` / `info proc mappings` region classification.
- `binary-memory-leak-vm-allocator-diagnosis` — heap-region and allocator
  clues when the fault address lands in a heap arena.
- `debugging-instrumentation-over-reasoning` — when to add instrumentation
  vs. extract more evidence from the core.
- `kernel-debugging-ftrace-kprobes-kdump` — kdump/vmcore analysis when the
  crash is in kernel space, not a userspace core.

## Evaluation

- Synthetic: `examples/bad/crash.c` must crash reproducibly under gdb with a
  garbage backtrace; `core_analyze.py` must report the 'A'-fill corruption and
  the unmapped fault address; `examples/good/clean_app.c` must exit 0 with
  correct output and "no crash evidence".
- False-positive: `clean_app.c` must never be flagged as a fault; mapped-but-
  unsymbolized frames must not be called corruption.
- Historical: Heartbleed-era memory analysis and Spectre-era crash post-
  mortems exercised the same extract-registers/maps/evidence loop this skill
  encodes.
- Adversarial: a corrupted-stack core with a garbage backtrace must be
  diagnosed via registers and maps (the `crash.c` fixture is exactly that
  case); a variant that overflows a different buffer must still be caught by
  the unmapped-return-address check.
- Commands and recorded results: `evals/README.md`.
