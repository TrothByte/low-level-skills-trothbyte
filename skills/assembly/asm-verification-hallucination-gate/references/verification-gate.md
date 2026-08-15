# Verification Hallucination Gate — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE(bad) →
COUNTEREXAMPLE(good) → VERIFICATION → SOURCE. Source ids refer to
registry/sources.yaml.

## 1. Verify the mnemonic exists before emitting it

- **RULE**: every mnemonic and pseudo-op you emit must be findable in the ISA
  manual for the exact target (`intel-sdm` Vol.2 for x86-64, `arm-arm` for
  Arm/Thumb-2). If you cannot name the page, you have invented the instruction.
- **WHY AI GETS IT WRONG**: generative models freely synthesize plausible
  mnemonics (`movqad`, `vpandqq`), and CDC COMPASS pseudo-ops (`JOB`, `SST`,
  `OCT`) were emitted verbatim in a transcribed failure; there is no mnemonic-
  level error-rate benchmark anywhere (the closest surrogate: IR→asm functional
  correctness is 44% x86_64 / 36% aarch64 — arxiv-2511-01183).
- **CORRECT REASONING**: a hallucinated instruction fails at assemble time —
  that failure is a feature. Treat "the assembler will reject it" as the
  expected outcome for any mnemonic you cannot cite.
- **EXAMPLE** (bad): `movqad %rax,%rbx` → `as: no such instruction`.
- **COUNTEREXAMPLE** (good): `movq %rbx,%rax` → assembles; `objdump` shows
  `48 89 d8`.
- **VERIFICATION**: `gcc -c bad.s` must exit 1 for the invented case; `gcc -c`
  + `objdump -d` for the real one.
- **SOURCE**: intel-sdm Vol.2 (instruction set reference); nasm-manual
  (syntax); arxiv-2511-01183 (44%/36% IR→asm correctness).

## 2. The gate is assemble → disassemble → compare bytes

- **RULE**: the ground truth for "is this encoded correctly" is the byte
  sequence that the assembler emits and the disassembler decodes back. Assemble
  the file, disassemble the object, and confirm the mnemonic + operands round-
  trip to what you intended. Never judge an encoding by reading the mnemonic.
- **WHY AI GETS IT WRONG**: models are byte-blind — they confidently invent
  facts about raw bytes they cannot actually read (e.g. resolving `call_indirect`
  targets or LEB128 indexes from raw bytes), and estimate instruction lengths
  from mnemonics instead of encodings.
- **CORRECT REASONING**: `gcc -c x.s` + `objdump -d x.o` is the parse+serialize
  round-trip that catches both nonexistent mnemonics and wrong operand order.
- **EXAMPLE** (bad): hand-written `.byte 0x69,0xC0,0x00,0x00,0x00,0x00` claimed
  as `imul eax,eax,38`; objdump decodes it as `imul $0x0,%eax,%eax` (immediate
  nulled — the BBoeOS PR#584 root cause).
- **COUNTEREXAMPLE** (good): `imull $38,%eax,%eax` → objdump `6b c0 26`.
- **VERIFICATION**: recorded in this skill's evals — `6b c0 26` vs `69 c0 00 00
  00 00`; both from real objdump runs.
- **SOURCE**: binutils-docs (objdump/as); intel-sdm Vol.2 (IMUL forms);
  arxiv-2407-02524 (14% exact-match disassembler).

## 3. AT&T: source comes first, immediates need `$`

- **RULE**: in AT&T, the operand order is source, destination. An immediate is
  written `$value`; a bare number is a memory address. Two memory operands are
  illegal for `mov`.
- **WHY AI GETS IT WRONG**: models trained mostly on Intel-syntax examples
  write `mov dest, src` and forget `$`; the transcribed failure emitted
  `movl 0x0, -0x4(%rbp)` meaning "store 0 to [rbp-4]", which is both reversed
  and missing `$`.
- **CORRECT REASONING**: `movl $0, -0x4(%rbp)` stores 0; `movl 0x0, -0x4(%rbp)`
  is rejected ("operand type mismatch") because it copies memory-to-memory.
- **EXAMPLE** (bad): `movl 0x0, -0x4(%rbp)` → `as: operand type mismatch for mov`.
- **COUNTEREXAMPLE** (good): `movl $0, -0x4(%rbp)` → `c7 45 fc 00 00 00 00`.
- **VERIFICATION**: `gcc -c` exit codes for both; objdump of the good one.
- **SOURCE**: binutils-docs (AT&T syntax); intel-sdm Vol.2 (MOV).

## 4. Register widths: AX is 16 bits, not 8

- **RULE**: AL/AH are 8-bit views of RAX; AX is the 16-bit view; EAX 32-bit;
  RAX 64-bit. The same holds for `bl/bh/bx/ebx`, `r8b/r8w/r8d/r8`, etc.
- **WHY AI GETS IT WRONG**: a recorded failure states "AX — 8-bit accumulator";
  models collapse register-size facts learned from C `char`/`short` intuition.
- **CORRECT REASONING**: writing `%al` only changes the low byte; the upper
  byte of `%ax` keeps its old value, which silently corrupts 16-bit counters.
- **EXAMPLE** (bad): `movl $0x1234,%eax` then `movb $0,%al` — ax ends as
  `0x1200`, not `0`.
- **COUNTEREXAMPLE** (good): `xorl %eax,%eax` clears all 64 bits; `movw $0,%ax`
  clears exactly 16.
- **VERIFICATION**: assemble + gdb single-step or the runtime check pattern in
  evals; objdump shows `b8 34 12 00 00` vs `31 c0`.
- **SOURCE**: intel-sdm Vol.1 §3.4.1; amd64-apm Vol.1.

## 5. Stack offsets are part of the encoding contract

- **RULE**: the stack offset in `disp(%rsp)` / `disp(%esp)` is the difference
  between the value's actual slot and the stack pointer at that exact moment
  (before or after the prologue, after each push). `(%rsp)` and `8(%rsp)` are
  different slots.
- **WHY AI GETS IT WRONG**: a merged PR introduced `mov ecx,[esp+4]` where
  `[esp]` was correct, silently writing products to the wrong slot; offset bugs
  assemble cleanly so nothing flags them.
- **CORRECT REASONING**: at a leaf function entry, `(%rsp)` holds the return
  address and `8(%rsp)` the first stack argument (Windows x64); choose the
  offset by walking pushes/pops, never from memory of a different function.
- **EXAMPLE** (bad): `movl 8(%rsp), %eax` when the intended value is at `(%rsp)`.
- **COUNTEREXAMPLE** (good): `movl (%rsp), %eax` for the top slot; runtime check
  confirms 1234 round-trips.
- **VERIFICATION**: runtime test in evals: entry-time `(%rsp)` ≠ `8(%rsp)`
  confirmed by exit code 0 of the good/bad pair.
- **SOURCE**: intel-sdm Vol.1 (stack); sysv-amd64-abi §3.2 (Windows x64 variant
  differs — shadow space, `%rcx` args); arxiv-2511-01183 context.

## 6. Multiple-operand IMUL: check which form you meant

- **RULE**: three-operand `imul r, r/m, imm` exists in imm8 and imm32 forms:
  `6b /r ib` (imm8) and `69 /r id` (imm32). Two-operand `imul r, r/m` multiplies
  by the same register (a square) — never a "times 1" no-op unless intended.
- **WHY AI GETS IT WRONG**: a real PR bug had `imul eax,eax,38` parse the second
  register as part of the immediate, nulling it — the emitted `69 c0 00 00 00 00`
  multiplies by zero, silently.
- **CORRECT REASONING**: the imm8 form `6b c0 26` is the canonical encoding of
  `eax*38`; if the bytes show a zero immediate, the operand got dropped.
- **EXAMPLE** (bad): `.byte 0x69,0xC0,0x00,0x00,0x00,0x00` → `imul $0x0,%eax,%eax`.
- **COUNTEREXAMPLE** (good): `imull $38,%eax,%eax` → `6b c0 26`.
- **VERIFICATION**: objdump -d on both objects — recorded in evals.
- **SOURCE**: intel-sdm Vol.2 (IMUL); binutils-docs; amd64-apm Vol.3.

## 7. Byte claims require a disassembler

- **RULE**: never assert what a byte sequence means without disassembling it.
  Encoding details like REX prefixes change the register silently: `8b 00` is
  `mov (%rax),%eax`, while `41 8b 00` is `mov (%r8),%eax`.
- **WHY AI GETS IT WRONG**: the byte-blind pattern — the model "knows" what
  bytes mean, reads them confidently, and is wrong in ways that look plausible
  (wrong operand, wrong width, invented instruction).
- **CORRECT REASONING**: pipe bytes through `objdump -d` (or your target
  disassembler) and compare mnemonics; eye-reading hex is not verification.
- **EXAMPLE** (bad): `.byte 0x8B,0x00` claimed as `mov (%r8),%eax`; objdump
  says `mov (%rax),%eax`.
- **COUNTEREXAMPLE** (good): `.byte 0x41,0x8B,0x00` → objdump `mov (%r8),%eax`.
- **VERIFICATION**: objdump -d on both objects — recorded in evals.
- **SOURCE**: intel-sdm Vol.2 (REX prefix); amd64-apm Vol.1 §1.5.1;
  arxiv-2407-02524 (decompiler hallucination context).

## 8. Calibration: "looks correct" is not evidence

- **RULE**: quantify trust before acting on generated assembly. Known baselines:
  IR→asm functional correctness 44% (x86_64) / 36% (aarch64) — arxiv-2511-01183;
  LLM superoptimizer test-passing 51.5% → 95.0% with RL — arxiv-2505-11480;
  disassembler exact match 14% — arxiv-2407-02524. Confidence without a machine
  gate is the failure mode.
- **WHY AI GETS IT WRONG**: the "high confidence" verdict in box64 #4214 was
  never verified by the author; in general, plausible-looking but wrong assembly
  is the dominant failure class.
- **CORRECT REASONING**: treat every emitted instruction as unverified until
  assemble + disassemble + (where relevant) execution pass; gate claims on
  reproducible tool output, not on style.
- **EXAMPLE** (bad): asserting a fix is correct because it "looks right" or the
  model "was very confident".
- **COUNTEREXAMPLE** (good): the evals/README.md verified-facts section with
  real exit codes and objdump bytes.
- **VERIFICATION**: re-run the recorded commands and compare byte-for-byte.
- **SOURCE**: arxiv-2511-01183; arxiv-2505-11480; arxiv-2407-02524;
  arxiv-2605-29059 (7% ideal decompilation, adversarial context).

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Mnemonic existence | must be citeable in the ISA manual, or the assembler rejects it |
| The gate | `gcc -c` → `objdump -d` → compare bytes; never read mnemonics |
| AT&T order | source first; `$` for immediates; two memory operands are illegal |
| AX width | AL/AH = 8, AX = 16, EAX = 32, RAX = 64 |
| Stack offsets | `(%rsp)` vs `8(%rsp)` are different slots; walk pushes/pops |
| IMUL 3-operand | imm8 `6b c0 26` vs nulled imm `69 c0 00 00 00 00` |
| Byte claims | always through a disassembler (REX changes the register) |
| Calibration | 44%/36% IR→asm, 51.5%→95% with RL, 14% exact-match disasm |
