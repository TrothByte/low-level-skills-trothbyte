# Decompilation Fidelity — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. Disassembly is an encoding, not the source

- **RULE**: a disassembler emits the machine instructions of one specific
  compilation. Many different C/C++ sources can compile to the same bytes, and
  one source compiles to different bytes at `-O1` vs `-O2`. The instruction
  stream therefore cannot uniquely determine the source.
- **WHY AI GETS IT WRONG**: reads the disassembly as a transcript of the
  original source and "reconstructs" variable names, types, and structure from
  it, then asserts that reconstruction is what the source was.
- **CORRECT REASONING**: the recoverable facts are instruction-level: widths,
  addressing forms, constants, control flow. Everything above that (signedness,
  aliasing, original loop bounds, variable lifetimes, structs) is inference.
  Same behavior, different encodings, is the norm: `-O1` and `-O2` fnv1a
  disassembly in `evals/README.md` differ but both return identical results.
- **EXAMPLE** (bad): "The `movzbl (%rcx),%edx` means the source was
  `h ^= (uint8_t)buf[i]`" — the type `uint8_t` is an inference; `int8_t` with
  different arithmetic is equally consistent with the encoding.
- **COUNTEREXAMPLE** (good): "The loop XORs a zero-extended byte into a 32-bit
  accumulator and multiplies by 0x01000193; the source cannot be determined
  uniquely, only behaviorally reproduced."
- **VERIFICATION**: `gcc -O1 -fno-inline -c` vs `gcc -O2 -fno-inline -c` on the
  same file; `objdump -d` shows different instruction streams; run both and
  compare behavior (identical).
- **SOURCE**: binutils-docs; empirical GCC 16.1.

## 2. The re-executability gate (LLM4Decompile)

- **RULE**: decompilation quality is measured by re-executability — the
  reconstructed program must compile and produce the same output as the
  original binary on the same inputs. It is a behavioral gate, not a
  readability score.
- **WHY AI GETS IT WRONG**: judges decompiled code by how "natural" or
  "correct-looking" it is; reports PASS on a reconstruction that was never
  compiled or never run against the reference.
- **CORRECT REASONING**: a reconstruction is a hypothesis with an executable
  test. Fix an input set, run both binaries, compare outputs. Any mismatch
  rejects the hypothesis regardless of how plausible the code looks. This is
  the metric that made LLM4Decompile (arxiv-2403-05286) abandon plausibility.
- **EXAMPLE** (bad): "This reconstruction is correct; I checked it reads the
  buffer byte by byte." No execution was performed.
- **COUNTEREXAMPLE** (good): "The reconstruction compiles with -Werror and
  prints 4c443831 / f713df32 for the fixture, matching the reference binary."
- **VERIFICATION**: `gcc -O2 plausible_wrong.c -o pw.exe && ./pw.exe` yields
  `f91dc7dd`/`daafe952` vs reference `4c443831`/`f713df32` — a mismatch the
  gate catches even though the code compiles cleanly with `-Wall -Wextra
  -Werror -O2`.
- **SOURCE**: arxiv-2403-05286.

## 3. Byte-round-trip is necessary, not sufficient

- **RULE**: the encoding-level round trip — disassembly reassembled to
  identical bytes/behavior — verifies the disassembler's correctness, not the
  decompiler's. A perfect byte round-trip says nothing about whether the
  reconstructed C matches the source.
- **WHY AI GETS IT WRONG**: presents a successful round-trip as proof the
  decompilation is faithful, conflating encoding fidelity with semantic
  fidelity.
- **CORRECT REASONING**: two gates are distinct: (a) byte/behavior round-trip
  (assembler level — necessary, cheap) and (b) re-executability of the
  reconstructed source (semantic level). Gate (a) passing while (b) fails is
  the common failure: the disassembly is fine, the decompiler's C is not.
- **EXAMPLE** (bad): "objdump round-trips perfectly, so the decompiled C is
  correct." Gate (a) passed; the C in `plausible_wrong.c` still fails gate (b).
- **COUNTEREXAMPLE** (good): report both gates separately: "round-trip: pass
  (reassembled binary prints identical output); reconstruction: fail (output
  differs on the same input)."
- **VERIFICATION**: `gcc -O2 -S checksum.c -o roundtrip.s; gcc roundtrip.s -o
  roundtrip.exe; ./roundtrip.exe` — identical to reference (gate a passes);
  `./pw.exe` diverges (gate b fails).
- **SOURCE**: arxiv-2407-02524 (round-trip framing); arxiv-2403-05286.

## 4. compile@k ≠ pass@k

- **RULE**: a candidate is not correct because it compiles. Benchmarks must
  separate compilation success from behavioral success; the gap between the two
  is the "plausible-but-wrong" population.
- **WHY AI GETS IT WRONG**: reports compile success as task success; uses
  "looks like it should run" as the completion criterion.
- **CORRECT REASONING**: at scale this gap is large: DEC-8 reports compile@k5
  up to 79.4% while pass@k is much lower; the divergence grows with function
  size and complexity, and collapses near the ~200-instruction cliff.
- **EXAMPLE** (bad): "All 10 decompiled functions compile, so the decompiler
  produced 10 correct results."
- **COUNTEREXAMPLE** (good): "8/10 compile; of those, only 3 also pass the
  behavioral check on the input set — the other 5 are plausible-but-wrong."
- **VERIFICATION**: `gcc -Wall -Wextra -Werror -O2 plausible_wrong.c` exits 0
  (compiles) while the program's output differs from the reference — recorded
  exit code 0 + mismatched output.
- **SOURCE**: arxiv-2607-06125.

## 5. The capability cliff near ~200 instructions

- **RULE**: decompilation quality degrades sharply as functions grow past
  roughly 200 instructions; models produce structurally plausible but
  behaviorally wrong output in this regime.
- **WHY AI GETS IT WRONG**: treats a small-function habit as universal; applies
  "I can read this function" confidence to 500-instruction functions.
- **CORRECT REASONING**: when a function exceeds the cliff, require execution
  as evidence. Do not rely on reading fluency; the empirical benchmark
  (arxiv-2607-06125) shows the cliff for exactly this regime.
- **EXAMPLE** (bad): reviewing a 300-instruction function from reading and
  declaring it equivalent without running either side.
- **COUNTEREXAMPLE** (good): "This function is 300+ instructions — I will not
  certify equivalence by reading; I will drive both binaries with a test set."
- **VERIFICATION**: count the instructions in the disassembly of the target
  (`objdump -d | wc -l`); if above ~200, gate every claim on execution.
- **SOURCE**: arxiv-2607-06125.

## 6. A clean disassembler output is not ground truth

- **RULE**: disassemblers decode whatever bytes they are pointed at. Data
  sections, padding, and misaligned streams decode into plausible-looking
  instructions. "It disassembled cleanly" is never evidence that the region is
  code or that the decode is right.
- **WHY AI GETS IT WRONG**: cites "objdump showed instructions" as proof of
  what a program does, without establishing that the bytes are code (section,
  entry point, call graph).
- **CORRECT REASONING**: section classification (`-j .rdata`, `.text`), symbol
  tables, and control-flow reachability decide what is code. objdump happily
  decodes `"fnv1a: %08x\n"` (bytes `66 6e 76 31 61 3a 20 25 30 38 78 0a`) into
  `data16 outsb (%rsi),(%dx)`, `jbe`, `(bad)`, `and $0xa783830,%eax`.
- **EXAMPLE** (bad): "The binary does an `and $0xa783830,%eax` right at
  startup" — actually a format string in `.rdata`.
- **COUNTEREXAMPLE** (good): "The bytes at 0x140004000 are in `.rdata`
  (section), not reachable as code; the `and` is data, not an instruction."
- **VERIFICATION**: `objdump -d -j .rdata checksum.exe` decodes the string
  bytes; `readelf -S`/`objdump -h` shows the section containing them.
- **SOURCE**: binutils-docs; empirical objdump 2.46.

## 7. Signedness and aliasing are not in the bytes

- **RULE**: `movzbl`/`movslq`/`imul` reveal the *encoding* the compiler chose,
  not the C type or aliasing rules. Two programs with different `int`/`uint`
  types or different aliasing assumptions can produce the same code.
- **WHY AI GETS IT WRONG**: writes `int32_t`/`uint32_t` into the
  reconstruction from the width of the instruction, then makes signedness
  claims the bytes cannot support.
- **CORRECT REASONING**: treat recovered types as hypotheses; only width and
  the extension mnemonic are directly visible. A "reconstruction" that changes
  only signedness is still a reconstruction; verify behaviorally.
- **EXAMPLE** (bad): "The `movzbl (%rcx),%edx` proves the source declared a
  `uint8_t` field" — the mnemonic proves the compiler zero-extended a byte;
  the original declaration is not recoverable from it.
- **COUNTEREXAMPLE** (good): "The accumulator is XORed with a zero-extended
  byte (movzbl). I model the byte as uint8_t; the source-level type beyond
  this is INFERRED."
- **VERIFICATION**: `plausible_wrong.c` crc32 uses the signed-byte variant and
  prints `daafe952` vs reference `f713df32` on data containing `0x80`/`0xFF`.
- **SOURCE**: intel-sdm (MOVSX/MOVZX); arxiv-2403-05286.

## 8. Confidence calibration (DeGPT, SCDBench)

- **RULE**: the empirical prior is that decompiled output is wrong. DeGPT's
  corrected fix rate is 37% (arxiv-2510-19615); SCDBench's ideal decompilation
  is 7% (arxiv-2605-29059); the Meta LLM Compiler reaches 14% exact match
  (arxiv-2407-02524). High-confidence, unverified verdicts are the failure mode.
- **WHY AI GETS IT WRONG**: presents un-gated decompilation with high
  confidence; treats "looks correct" as evidence.
- **CORRECT REASONING**: prior = wrong. Every statement carries a gate status:
  VERIFIED (behaviorally tested) / KNOWN (documented fact) / INFERRED
  (reconstruction guess) / UNVERIFIED (untested).
- **EXAMPLE** (bad): "I am confident this decompiled function is exactly the
  original `fnv1a`."
- **COUNTEREXAMPLE** (good): "This reconstruction is INFERRED; I will VERIFY by
  running both on the fixture input set before any use."
- **VERIFICATION**: compare `plausible_wrong.c` (compiles cleanly) with the
  reference — mismatched output shows the confidence would have been wrong.
- **SOURCE**: arxiv-2510-19615; arxiv-2605-29059; arxiv-2407-02524.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Source recovery | bytes ↔ many sources; `-O1` vs `-O2` differ, behavior identical |
| Re-executability | compile reconstructed C; run; outputs must match the reference |
| Round-trip | byte round-trip proves the disassembler, not the decompiler |
| compile@k | compiling ≠ passing; gap is the plausible-but-wrong population |
| Cliff | >~200 instructions: require execution, not reading |
| Clean output | objdump decodes data as code; sections decide what is code |
| Types | width/extension visible; signedness & aliasing are inference |
| Prior | DeGPT CFR 37%, SCDBench 7%, Meta exact 14% — assume wrong until gated |
