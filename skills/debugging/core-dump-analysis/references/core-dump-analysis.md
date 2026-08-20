# Core Dump Analysis — Reference

Deep reference for the `core-dump-analysis` skill. The SKILL.md carries the
discipline; this file carries the mechanics: how cores are produced on each
OS, how to load them, what each extraction command returns, and how to
interpret registers, fault addresses, memory maps, and heap regions.

## 1. What a core is

A core dump is a snapshot of a process's memory and execution state at the
moment of a crash: the register set of every thread, the mapped memory
regions, the signal that killed it, and (usually) the fault address. Because
it is a snapshot, it is deterministic evidence: you can re-ask the same
questions repeatedly, which you cannot do with a live debugging session on an
intermittent crash.

The core is only as good as its provenance. The two variables that make a
core useful are (a) the exact binary that produced it and (b) its debug info.
Without the exact binary, addresses are meaningless; without symbols, frames
reduce to `??`.

## 2. Obtaining a core

### Linux

- Enable kernel-written cores: `ulimit -c unlimited` (shell limit) and
  `core_pattern`. `cat /proc/sys/kernel/core_pattern` shows where cores land
  and whether a pipe handler (e.g. systemd-coredump, `|/usr/lib/systemd/
  systemd-coredump`) intercepts them. A literal `core` means "core in the
  working directory"; paths like `/var/crash/core.%e.%p` or a pipe to
  coredumpctl change that.
- systemd systems: `coredumpctl list`, `coredumpctl info <pid>`,
  `coredumpctl gdb <pid>` — extracts the core and launches gdb with the
  correct binary.
- gdb-driven: within a session, `generate-core-file <file>` writes a core
  from the inferior's current state (works even without kernel coredump
  setup).
- `gcore <pid>` — the CLI wrapper for `generate-core-file`.

### Windows

- WinDbg/cdb: `.dump /ma <file>` writes a full minidump. `/ma` = full memory
  with all regions, which this skill needs for maps inspection.
- ProcDump: `procdump -ma -e app.exe app.dmp` captures a full dump on an
  unhandled exception; `-ma -t` captures on process termination.
- gdb (MinGW): `generate-core-file` is NOT supported on Windows targets
  (gdb 17.2, measured 2026-08-20: "Can't create a corefile").
- Windows minidumps are read by WinDbg/cdb, and also by gdb for the native
  subset (`gdb app.exe crash.dmp` works for `.dmp` files on some builds).

## 3. Loading the core

```
gdb ./app core            # interactive
gdb -batch -ex "set pagination off" -ex "thread apply all bt" ./app core
gdb -batch -ex "set pagination off" -ex "bt" -ex "info registers" \
    -ex "info proc mappings" -ex "x/16wx $rsp" ./app core.dump
```

`set pagination off` is mandatory for batch mode — pagination would stop the
output and hang the run. Windows gdb lacks `info proc mappings`
("Not supported on this target.") — use `info files`, which lists every
loaded module's sections with address ranges (the recorded output in
`evals/recorded/gdb_live.txt` uses it).

## 4. Backtrace extraction

| Command | Gives you |
|---|---|
| `bt` | backtrace of the selected thread |
| `bt full` | backtrace + arguments and local variables per frame |
| `thread apply all bt` | backtrace of every thread |
| `info threads` | thread list with ids |
| `frame N` | switch to frame N |

The signal line tells you which thread faulted: on Linux, `Thread N received
signal SIGSEGV`; that N is the faulting thread. On Windows, check each
thread's exception record (`.ecxr` in WinDbg) for the one with the access
violation.

On multi-threaded cores, do `thread apply all bt` FIRST. The faulting thread
is frequently not the one the agent would guess (I/O worker vs. main).

## 5. Registers

`info registers` (or `info registers rip rsp rbp` for a subset) gives the
register set of the currently selected thread — i.e. the faulting thread's
context.

- `rip`/`eip`/`pc` — the faulting (or last-executed) instruction address.
  Is it inside a mapped executable region? Is it symbolizable (`info symbol
  <addr>`, or the `<func+N>` suffix gdb prints)? If rip points at
  `ucrtbase!strlen+16` while the instruction stream never reached a valid
  call, something else was dereferenced.
- `rsp`/`esp`/`sp` — current stack pointer. Should be within a stack region
  (Linux `info proc mappings` shows `[stack]`; Windows `info files` does not
  list the stack, so on Windows gdb treat the register value as evidence on
  its own and cross-check with `x/` inspection).
- `rbp`/`ebp`/`fp` — frame pointer; the chain frame-pointer → saved frame
  pointer can reconstruct a stack that the return-address walker lost.
- Argument registers (`rdi`, `rsi`, `rdx`, `rcx`) — the pointer that was
  being dereferenced is usually in one of these or in rax (the result of a
  preceding call). In the recorded crash, `rax = 0x4141414141414141` while
  `strlen` reads `user_input` — the dereferenced address itself is the
  corrupted payload.

## 6. The fault address (si_addr)

On Linux, the signal's siginfo carries the address of the faulting access:

```
p $_siginfo        # gdb convenience: view siginfo
p $_siginfo._sifields._sigfault.si_addr
info signals       # signal handling dispositions
```

Fault codes distinguish the kind of access:
`SEGV_MAPERR` (address not mapped at all) vs `SEGV_ACCERR` (mapped but access
not permitted — e.g. write into read-only page).

The fault address must be cross-checked against the register dump: the
accessing instruction plus the offending register value. On Windows, the
`.exr`/`!analyze -v` in WinDbg reports the faulting address the same way.

## 7. Memory maps

`info proc mappings` (Linux) lists every region with base, end, permissions,
offset, and label. `info files` (Windows gdb) lists module sections. Either
way you get what you need for classification:

- fault address inside a module → the access was in that module's memory;
- fault address in a `[heap]`/data region → heap or data corruption;
- fault address in `[stack]` or near rsp → stack problem;
- fault address in NO region → the address itself was garbage (classic
  poisoned pointer, e.g. `0x4141414141414141`).

ASLR means the load base changes every run. Never reuse a base address from a
previous session — always derive the mapping from the current core.

## 8. Stack integrity validation

Validate every return address in the backtrace against the mappings:

1. Frame addresses must fall inside mapped regions. If frames 2..n are all
   `0x4141414141414141` (ASCII 'A'), the return address was overwritten —
   this is a buffer overflow signature, and the frames are payload, not call
   history.
2. Frames may be mapped but unsymbolized (`??`): possible but not proof of
   corruption; check `info symbol` on those addresses.
3. If the stack walker is lost: `set backtrace past-main on`, `frame 0`,
   then inspect rbp chain, look for the last symbolizable return address, or
   switch to alternate stacks (other threads' stacks may still be intact).
4. The corrupting write usually lives BELOW the fault in the stack dump:
   scan `x/` output for the start of the fill pattern to find the buffer that
   overflowed.

## 9. Heap clues

- Locate the heap region in `info proc mappings` (`[heap]` on Linux;
  `ntdll!RtlAllocateHeap` arenas on Windows).
- If the fault address falls inside a heap region, check the chunk header:
  the size field and the `prev_size`/flags bytes. `0x41414141` in the size
  field means the header was overwritten too.
- `info malloc` (some glibc builds with `maintenance` support) may show arena
  state; otherwise walk chunks manually from the heap base.
- Corrupted backtrace + fault in a heap region is evidence of heap metadata
  corruption; fault outside any region with clobbered return addresses is
  evidence of stack overflow. Do not blur the two.

## 10. Correlating to a root cause

The complete evidence set: siginfo (fault address + code) + registers
(faulting instruction + offending pointer) + memory maps (region
classification) + symbols (function names). Combine them into a hypothesis
about the WRITER, not the reader: find the last code that touched the
corrupted region before the fault, then verify with source inspection. The
faulting function is usually the victim, not the cause.

## 11. Windows specifics (WinDbg/cdb)

| Linux | Windows equivalent |
|---|---|
| `gdb ./app core` | `windbg -z app.dmp` / `cdb -z app.dmp` |
| `bt` | `kp`, `~* kp` (all threads) |
| `info registers` | `r`, `.ecxr` (exception context), `r rip` |
| `info proc mappings` | `!address`, `lmv m*` (module map) |
| fault address | `.exr` / `!analyze -v` |
| heap | `!heap`, `!heap -p -a <addr>` |

`!analyze -v` automates fault address + exception record + stack validation —
run it, then still verify its frames against `!address` before trusting them.

## 12. Worked example from the recorded host output

`examples/bad/crash.c` overflows an 8-byte stack buffer, clobbering the saved
return address with 'A' (0x41) fill. Under gdb it faults in `ucrtbase!strlen`
reading `0x4141414141414141`. The extracted evidence set (see
`evals/recorded/gdb_live.txt` and `postmortem.txt`):

- signal: SIGSEGV on thread 1;
- rip: `ucrtbase!strlen+16`, symbolizable, inside ucrtbase .text;
- rax / dereferenced pointer: `0x4141414141414141` — unmapped, "Cannot access
  memory";
- backtrace: frames 2-7 all `0x4141414141414141 in ?? ()` — outside every
  mapped region = corrupted stack, 'A'-fill signature;
- rsp = 0x5ffe08; `x/24wx $rsp-0x40` shows the overflowed stack area below
  the fault;
- verdict (from `core_analyze.py`): 7 of 11 return addresses outside mapped
  regions, 'A' fill → buffer overflow / overwritten return address.

The backtrace is payload; the register + maps evidence is the fact.
