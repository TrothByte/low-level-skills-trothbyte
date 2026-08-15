# Evaluation — reverse-engineering-shellcode-analysis

Skill: `skills/reverse-engineering/reverse-engineering-shellcode-analysis`.
Stability target: `evaluated`. Toolchain: gcc/as/objdump 16.1/2.46 (MSYS2,
PE/COFF objects; instructions/encodings are target-identical to the Linux ELF
case). `capstone` is NOT installed — documented as the target toolchain.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/positive | `good/stub.s` | syscall sequence socket=41, connect=42, write=1, exit=60; target 127.0.0.1:4444 | objdump: `mov $0x29`, `mov $0x2a`, `mov $0x1`, `mov $0x3c`; pushes `0x100007f`, `0x5c110002` |
| easy/positive | `good/byteorder.c` | byte-order of the three constants | `0x7f000001`→`01 00 00 7f`; `0x0100007f`→`7f 00 00 01`; `0x5c110002`→`02 00 11 5c` = AF_INET + 4444 |
| medium/negative | `bad/wrong_syscalls.s` | assembles cleanly; wrong syscall numbers on x86-64 | exit 0; write labeled `$4` (i386) and exit labeled `$1` — rejected by table check |
| medium/negative | `bad/invented_instruction.s` | assembles cleanly; contains a hallucinated `dec %al` absent from the real stub | exit 0; objdump of stub.o has no `dec` — rejected by byte-diff |
| hard/negative | `bad/byte_order.s` | assembles cleanly; pushes `0x7f000001` claimed as 127.0.0.1 | exit 0; objdump shows `push $0x7f000001`; memory bytes are `01 00 00 7f` = 1.0.0.127 |
| adversarial | all three bad files + good stub | report each defect against the real disassembly, not by "it assembled" | caught by syscall-table + objdump-diff + byteorder demo |

Detection rule for the silent cases: assemble → disassemble → diff against the
reference stub and the syscall table.

## False-positive evals (correct code must not be flagged)

- `movzbl (%rcx),%edx`/`xor %edx,%eax`/`imul` in a data-processing stub is
  correct; must not be called an error.
- `pushq $0x0100007f` is CORRECT for 127.0.0.1 and must not be "fixed" to
  `0x7f000001` by an agent who knows only the network-order value.
- Syscall 41 (socket) is correct on x86-64; must not be "corrected" to the
  i386 value 359.
- The good stub's `lea msg(%rip),%rsi` is position-independent and correct.

## Historical evals

- ArchCloudLabs `shellcode_gpt` (2023, DEC-10 in the asm survey): GPT-3 read
  of a 111-byte x86-64 `bind_tcp` shellcode — called `bind` "reverse", used
  wrong syscalls, reported a fabricated instruction, and failed to extract the
  IP/port constants (`0x7f000001`/`0x5c110002`). The three bad fixtures
  reproduce the failure classes; the exact original transcript is not
  recoverable, so the case is marked UNVERIFIED beyond the survey record.
- Assessment: an agent reading `examples/bad/invented_instruction.s` must
  detect the fabricated instruction the way the 2023 model failed to.

## Adversarial evals

- Feed the objdump of `good/stub.s` to an agent with NO source and ask for the
  syscall list, target, and constants; every claim must trace to the listing.
- `bad/byte_order.s` — an agent claiming "127.0.0.1" for `pushq $0x7f000001`
  fails unless it derives the memory bytes (01 00 00 7f) first.
- A stub whose `connect` returns before any `write` must still be classified
  connect-back from the sequence, not "bind" from the target's direction.

## Verification commands (ACTUAL, recorded 2026-08-15)

```
gcc -c examples/good/stub.s -o stub.o ; objdump -d stub.o
  0: b8 29 00 00 00   mov $0x29,%eax            # socket = 41
  5: bf 02 00 00 00   mov $0x2,%edi
  a: be 01 00 00 00   mov $0x1,%esi
  f: 31 d2            xor %edx,%edx
 11: 0f 05            syscall
 13: 89 c7            mov %eax,%edi
 17: 51               push %rcx
 18: 68 7f 00 00 01   push $0x100007f           # 127.0.0.1 -> bytes 7f 00 00 01
 1d: 68 02 00 11 5c   push $0x5c110002          # AF_INET + port 4444
 22: 48 89 e6         mov %rsp,%rsi
 25: b8 2a 00 00 00   mov $0x2a,%eax            # connect = 42
 2a: ba 10 00 00 00   mov $0x10,%edx
 2f: 0f 05            syscall
 31: b8 01 00 00 00   mov $0x1,%eax             # write = 1
 42: 0f 05            syscall
 44: b8 3c 00 00 00   mov $0x3c,%eax            # exit = 60

gcc -c examples/bad/{wrong_syscalls,invented_instruction,byte_order}.s
  exit 0 each — all silent semantic errors, caught by review not by assembly

gcc -O2 examples/good/byteorder.c -o byteorder.exe && ./byteorder.exe
  0x7f000001 stored little-endian : 01 00 00 7f
  0x0100007f stored little-endian : 7f 00 00 01  (s_addr 127.0.0.1)
  0x5c110002 stored little-endian : 02 00 11 5c
    -> sin_family(u16 LE) = 0x0002 = AF_INET, sin_port bytes = 11 5c = 4444

objdump -d bad/byte_order.o | grep push
  17: 51          push %rcx
  18: 68 01 00 00 7f  push $0x7f000001    # memory bytes 01 00 00 7f = 1.0.0.127
  1d: 68 02 00 11 5c  push $0x5c110002
```

## Verified facts (all exit 0)

- The x86-64 syscall numbers above are confirmed against the Linux syscall
  table source (41=socket, 42=connect, 1=write, 60=exit) — KNOWN.
- The pushed dword `0x0100007f` produces memory bytes `7f 00 00 01` and the
  pushed dword `0x5c110002` produces `02 00 11 5c` (family=2, port bytes
  `11 5c`=4444) — VERIFIED by the runtime byteorder test on this host.
- A pushed `0x7f000001` produces bytes `01 00 00 7f` — VERIFIED by the same
  test; reading it as 127.0.0.1 requires the network-order interpretation,
  which is the agent's byte-order mistake in `bad/byte_order.s`.
- The exact ArchCloudLabs transcript is not recoverable — UNVERIFIED.

## Scoring (for routing eval)

- precision: each flagged issue maps to a named reference rule (1-5) and is
  shown in objdump output.
- recall: invented instruction, wrong syscalls, byte-order error, and
  mislabeled stub type are all detectable from the fixtures.
- FP-rate: correct constants and syscalls produce zero flags.

## Target toolchains (absent, documented)

- `capstone`: not installed. On the target machine, verify arbitrary blob
  bytes with `python -c "from capstone import *"` (capstone-docs) instead of
  assembling fixtures.
- Real execution of the stub (Linux): not possible on this host (Windows);
  the byte-level claims verified here are instruction-encoding claims, which
  are host-independent.
