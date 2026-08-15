# Shellcode Analysis — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. The instruction inventory comes from objdump, not from reading

- **RULE**: the authoritative instruction list of a blob is produced by
  disassembling its exact bytes. Any instruction claimed that does not appear
  in that listing is invented, regardless of how plausible it looks.
- **WHY AI GETS IT WRONG**: models "read" shellcode by pattern-matching
  familiar sequences and fill gaps with hallucinated instructions — the
  2023 ArchCloudLabs `shellcode_gpt` read of a 111-byte x86-64 `bind_tcp`
  reported an instruction described as "decrements AL" that is absent from
  the bytes. `dec %al` is a real instruction, so the fabricated transcript
  assembles cleanly, hiding the error.
- **CORRECT REASONING**: run the bytes through a disassembler, then diff the
  transcription against it. If the transcript has one instruction the listing
  does not, the whole transcript is suspect (DEC-10 class).
- **EXAMPLE** (bad): a transcript that inserts `dec %al` between `mov %eax,
  %edi` and `xor %ecx,%ecx` — see `examples/bad/invented_instruction.s`.
- **COUNTEREXAMPLE** (good): the transcript equals `objdump -d stub.o` line
  for line; the only differences are assembly directives and labels.
- **VERIFICATION**: `gcc -c invented_instruction.s` exits 0; `objdump -d
  stub.o` shows no `dec` anywhere — the diff is the test.
- **SOURCE**: binutils-docs (objdump); survey record of ArchCloudLabs
  shellcode_gpt (single-case transcript; exact bytes UNVERIFIED).

## 2. Syscall numbers are per-architecture, taken from the table

- **RULE**: Linux syscall numbers differ between 32-bit and 64-bit modes.
  On x86-64 (`syscall_64.tbl`): read=0, write=1, socket=41, connect=42,
  bind=49, listen=50, accept4=288, dup2=33, execve=59, exit=60. On i386
  (`syscall_32.tbl`): write=4, exit=1, socket=359, bind=361, connect=362.
  Confusing the two is the single most common "wrong syscall" error.
- **WHY AI GETS IT WRONG**: writes from memory of older tutorials (i386) or
  from the wrong-architecture entry; every instruction assembles, so the error
  is invisible to the assembler.
- **CORRECT REASONING**: for every `syscall` instruction, the value in `rax`
  must be matched against the target architecture's table, not assumed.
- **EXAMPLE** (bad): `movl $4, %eax; syscall` labeled "write()" — on x86-64
  syscall 4 is `stat`, and the fd for write never appears in `rdi`.
- **COUNTEREXAMPLE** (good): `movl $1, %eax` + fd in `%edi` + buffer in `%rsi`
  + length in `%edx` = write(1) per syscall_64.tbl.
- **VERIFICATION**: `gcc -c wrong_syscalls.s` exits 0; cross-checking the
  immediates against the table rejects it.
- **SOURCE**: linux-syscall-table.

## 3. IP/port constants require byte-order reasoning, not pattern matching

- **RULE**: an IPv4 address constant is meaningless without stating the byte
  order it is used in. The network-order (big-endian) value of 127.0.0.1 is
  `0x7f000001`. But a 32-bit immediate pushed or stored on little-endian x86
  occupies memory LSB-first: the dword `0x7f000001` becomes bytes
  `01 00 00 7f` = s_addr 1.0.0.127. To place 127.0.0.1 into memory the dword
  must be `0x0100007f` (bytes `7f 00 00 01`).
- **WHY AI GETS IT WRONG**: sees `0x7f000001` and writes "127.0.0.1" without
  checking storage order (DEC-10: IP/port never extracted at all; when
  attempted, byte order is guessed).
- **CORRECT REASONING**: enumerate the interpretations: (a) as a value —
  network-order `0x7f000001` is 127.0.0.1; (b) as little-endian memory bytes —
  `01 00 00 7f` which is 1.0.0.127. Then state which the code does with it.
  Ports: bytes `11 5c` are network-order u16 0x115c = 4444.
- **EXAMPLE** (bad): `pushq $0x7f000001` claimed to load 127.0.0.1 — memory
  bytes are `01 00 00 7f`, i.e. 1.0.0.127.
- **COUNTEREXAMPLE** (good): `pushq $0x0100007f` → bytes `7f 00 00 01`
  (127.0.0.1); `pushq $0x5c110002` → bytes `02 00 11 5c` = AF_INET(0x0002) +
  port bytes `11 5c` = 4444.
- **VERIFICATION**: `byteorder.exe` prints the memory bytes of all three
  constants (recorded in `evals/README.md`).
- **SOURCE**: intel-sdm (endianness of memory operands); empirical runtime
  test on this host.

## 4. Stub type (bind vs reverse) follows from the syscall sequence

- **RULE**: the behavior of a network stub is read from its syscall sequence:
  bind→listen→accept = bind/listen server; socket→connect = reverse
  (connect-back); dup2×3 + execve = shell handover. Naming is derived, not
  guessed from layout.
- **WHY AI GETS IT WRONG**: labels by familiarity ("bind named reverse" in
  DEC-10), or by the target's direction ("any IP constant = reverse shell").
- **CORRECT REASONING**: list the syscalls in order with their arguments, then
  classify. A connect-back stub must contain `connect`; a bind stub must
  contain `bind`/`listen`/`accept`.
- **EXAMPLE** (bad): a stub containing bind(49), listen(50), accept4(288)
  reported as "a reverse shell to 127.0.0.1".
- **COUNTEREXAMPLE** (good): "socket(41) → connect(42) → write(1) → exit(60):
  connect-back client that writes a banner to the remote end."
- **VERIFICATION**: objdump the fixture and classify from the syscall numbers
  actually present.
- **SOURCE**: linux-syscall-table; intel-sdm.

## 5. Assembly success is not byte-identity

- **RULE**: `as`/`gcc -c` proves syntactic validity, nothing else. Wrong
  syscall numbers, extra instructions, and wrong constants all assemble.
  The analysis artifact must match the blob's disassembly, not merely assemble.
- **WHY AI GETS IT WRONG**: uses "it compiles/assembles" as the verification
  step and stops there.
- **CORRECT REASONING**: two different tests: (a) assembles (trivial), (b)
  objdump of the artifact equals objdump of the blob modulo labels
  (byte-identity of the instruction stream).
- **EXAMPLE** (bad): "The transcript is correct — I assembled it." It
  assembles AND contains an extra `dec %al`.
- **COUNTEREXAMPLE** (good): "objdump of my transcript and objdump of the blob
  are identical in the .text section."
- **VERIFICATION**: `gcc -c` exit code 0 for all three bad examples; the
  objdump diff is the discriminating test.
- **SOURCE**: binutils-docs.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Instruction inventory | objdump of the exact bytes is ground truth; anything extra is invented |
| Syscall numbers | x86-64: write=1, socket=41, connect=42, bind=49, exit=60 (syscall_64.tbl) |
| IP constants | `0x7f000001` = value 127.0.0.1; pushed LE → bytes `01 00 00 7f` = 1.0.0.127 |
| Port packing | `0x5c110002` → bytes `02 00 11 5c` → AF_INET + port 4444 |
| Stub type | bind→listen→accept vs socket→connect; classify from the sequence |
| Assembly | assembles ≠ byte-identical; diff objdump output |
