---
name: reverse-engineering-shellcode-analysis
description: Use when reading, verifying, or extracting information from raw shellcode bytes (x86-64 Linux focus): syscall numbers, IP/port constants, instruction inventory. Prevents invented instructions, wrong syscall tables, and byte-order errors in IP/port extraction.
---

# Reverse Engineering: Shellcode Analysis

## When to use

- Reading a raw shellcode blob (bytes, no source) and reporting what it does.
- Extracting the target IP/port and syscall behavior from connect-back/bind stubs.
- Verifying an LLM's (or your own) transcription of shellcode against the bytes.
- Comparing two shellcode transcripts byte-for-byte.

## When not to use

- Decompiling a normal compiled binary to C — use
  `binary-disassembly-decompilation-fidelity`.
- Recovering types/structs from a PE/ELF with symbols — use
  `binary-analysis-type-recovery`.
- Writing a working exploit or payload — this skill is analysis discipline,
  not payload construction.
- AArch64/RISC-V/32-bit shellcode — different instruction sets and syscall
  tables; verify with the target architecture's table.

## What the agent often gets wrong

- Inventing instructions: the 2023 ArchCloudLabs `shellcode_gpt` read of a
  111-byte x86-64 `bind_tcp` claimed a "decrements AL" instruction that is not
  in the bytes — there is no such mnemonic, and no `dec` of `%al` at that
  offset. The instruction inventory must match `objdump` byte-for-byte.
- Using the wrong syscall table: mixing i386 numbers (write=4, exit=1,
  socket=359) into x86-64 analysis (write=1, exit=60, socket=41). Both
  assemble; only one is correct for the target.
- Labeling `bind` as "reverse" or misidentifying the stub type (DEC-10).
- Misreading IP/port constants: `0x7f000001` is read as "127.0.0.1 stored
  directly" without checking that the value stored to little-endian memory
  produces bytes `01 00 00 7f` — which is 1.0.0.127, not localhost. The
  pushed dword for 127.0.0.1 must be `0x0100007f` (memory bytes `7f 00 00 01`).
- Believing a transcript because it assembles: `dec %al` assembles fine; so do
  wrong syscall numbers. Assembly success is not byte-identity with the blob.

## How to reason correctly

1. Disassemble the exact bytes (`objdump -D -b binary -m i386:x86-64`, or
   `as`/`objcopy` on the fixture); the instruction inventory is the objdump
   output, nothing more.
2. Cross-check every syscall number against the target's table
   (`linux-syscall-table`, syscall_64.tbl): write=1, exit=60, socket=41,
   connect=42, bind=49, listen=50, accept4=288, dup2=33, execve=59.
3. For every 32-bit immediate, list the byte-order interpretations:
   little-endian memory bytes vs big-endian (network-order) value; then map to
   the field layout (sockaddr_in: u16 family, u16 port, u32 addr).
4. Only then name the behavior (bind vs reverse/connect-back) from the syscall
   sequence, not from "what shellcode like this usually does".
5. If a claim (instruction, syscall, constant) does not appear in the recorded
   disassembly, mark it INVENTED/UNVERIFIED and reject it.

## What to verify

- The transcription's instruction sequence equals `objdump` output exactly
  (no added/removed/reordered instructions).
- Every syscall number matches the target architecture's table.
- Every IP/port constant is extracted with its byte order stated and the
  sockaddr field layout named.
- The stub type (bind/listen/accept vs connect-back) follows from the syscall
  sequence actually present.

## How to verify

```
gcc -c stub.s -o stub.o && objdump -d stub.o     # record real instructions
gcc -c wrong_syscalls.s -o w.o                   # assembles! but wrong syscalls
gcc byteorder.c -o byteorder.exe && ./byteorder.exe
```

For arbitrary blobs: `objcopy -I binary -O pe-x86-64 blob.bin blob.o` then
`objdump -d`, or use capstone from Python on the target machine
(`capstone-docs`). Recorded byte-order outputs: `evals/README.md`.

## Where the knowledge comes from

- `linux-syscall-table` — syscall_64.tbl: authoritative syscall numbers per
  architecture.
- `intel-sdm` — instruction semantics, encoding, and register/width rules.
- `capstone-docs` — multi-architecture disassembly API for blobs on the
  target machine.
- `binutils-docs` — objdump `-d`/`-D` and binary-file handling.
- Empirical: gcc/as/objdump 16.1/2.46 (MSYS2), recorded 2026-08-15.
- Historical: ArchCloudLabs `shellcode_gpt` (2023) — GPT-3 read of a 111-byte
  bind_tcp stub: invented instruction, wrong syscalls, unextracted
  IP/port; the case is single-sourced and the exact transcript is not
  recoverable (UNVERIFIED beyond the survey record).

## Related skills

- `binary-disassembly-decompilation-fidelity` — same "verify against the
  bytes" discipline for decompiled C
- `asm-x86-64-registers-and-addressing` — encodings, immediates, suffix rules
- `asm-signed-unsigned-branches` — signed/unsigned reading of shellcode
  comparisons
- `reverse-engineering-ghidra-agent-automation` — triage/annotation loop when
  shellcode is embedded in a larger binary

## Evaluation

Synthetic: read `examples/good/stub.s` and report the syscall sequence
(socket 41, connect 42, write 1, exit 60), target (127.0.0.1:4444), and both
constants with byte order stated; the byte-order demo
(`examples/good/byteorder.c`) must produce `01 00 00 7f` for `0x7f000001` and
`7f 00 00 01` for `0x0100007f`.
False-positive: `movzbl`/`xor`/`imul` must NOT be misread; the correct stub
must not be "corrected".
Historical: ArchCloudLabs `shellcode_gpt` — reproduce the three failure
classes (invented instruction, wrong syscalls, missing IP/port) and reject
them.
Adversarial: `examples/bad/wrong_syscalls.s` (i386 numbers on x86-64),
`examples/bad/invented_instruction.s` (hallucinated `dec %al`), and
`examples/bad/byte_order.s` (`pushq $0x7f000001` claimed as localhost) must
each be caught against the real disassembly.
Commands and verified facts: `evals/README.md`.
