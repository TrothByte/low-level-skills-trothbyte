# Evaluation — binary-disassembly-decompilation-fidelity

Skill: `skills/binary-analysis/binary-disassembly-decompilation-fidelity`.
Stability target: `evaluated`. Toolchain: GCC 16.1.0, objdump/readelf 2.46
(MSYS2 MinGW, PE/COFF). Reference source: `examples/good/checksum.c`.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/positive | `good/checksum.c` compiled `-O2` | run prints reference hashes | `fnv1a: 4c443831`, `crc32: f713df32` |
| medium/negative | `bad/plausible_wrong.c` | compiles cleanly BUT outputs differ from reference | compiles exit 0; `fnv1a: f91dc7dd`, `crc32: daafe952` — both wrong |
| medium/positive | round-trip: `gcc -O2 -S` then assemble | reassembled binary behaves identically | exit 0; `fnv1a: 4c443831`, `crc32: f713df32` |
| medium/positive | `-O1` vs `-O2` disassembly | different instruction streams, same behavior | O1 has a length-check prologue and `%r8`-based loop; O2 uses `lea`-bound loops — both print the same hashes |
| hard/negative | record disassembly, then reconstruct C | any reconstruction must pass re-executability | `plausible_wrong.c` fails the gate (see above) |
| adversarial | disassembly with `imul $0x1000193`, `and $0xedb88320` | wrong constant or wrong precedence must be rejected by the behavioral gate | precedence error in `plausible_wrong.c` is caught by output mismatch |

Detection rule for silent cases: the reviewer must run the reconstruction and
compare outputs — a clean compile is not evidence (rule 4).

## False-positive evals (correct code must not be flagged)

- `good/checksum.c` itself is correct; recompiling it is NOT "plagiarism" and
  identical behavior is expected.
- A legitimate `movzbl (%rcx),%edx` in a correct function must NOT be flagged
  as "truncation" or "signedness bug".
- `objdump -d -j .rdata` on string bytes (`data16 outsb`, `jbe`, `(bad)`,
  `and $0xa783830,%eax`) must NOT be reported as program behavior — the bytes
  are data, not code.
- A reconstruction that is behaviorally verified (both hashes match) must NOT
  be rejected just because it differs stylistically from the original source.

## Historical evals (documented incidents, from sources.yaml)

- DeGPT (arxiv-2510-19615): "corrected fix rate" 37% — ~63% of lines the
  model "fixes" in a decompilation remain wrong. A fixer agent that reports
  "fixed" without re-executing is repeating DEC-3.
- SCDBench (arxiv-2605-29059): 7% ideal decompilation (42/600); output "looks
  compilable and plausible, semantics diverge".
- Meta LLM Compiler (arxiv-2407-02524): asm→IR 45% round-trip, 14% exact
  match; disassembly→IR is a lossy projection, not a transcription.
- Capability cliff (arxiv-2607-06125): compile@k5 up to 79.4% while pass@k is
  far lower; functions past ~200 instructions degrade.
- REx86 (arxiv-2510-20975): base LLMs hallucinate comments attached to x86
  disassembly — a clean comment is not analysis.

## Adversarial evals

- `bad/plausible_wrong.c` has TWO seeded defects (precedence, signed byte);
  the agent must locate both via the recorded disassembly + behavioral gate,
  not by code review alone.
- A "reconstruction" that reads `movzbl` but uses `(int8_t)` promotion must be
  rejected (the disassembly shows zero-extension).
- An input containing bytes >= 0x80 must be part of the test set; without it,
  the signed-byte defect is invisible.
- Data-in-.rdata presented as code (rule 6) must be recognized as a
  disassembler artifact.

## Verification commands (ACTUAL, recorded 2026-08-15)

```
gcc -O2 checksum.c -o checksum.exe && ./checksum.exe
  fnv1a: 4c443831
  crc32: f713df32

gcc -Wall -Wextra -Werror -O2 plausible_wrong.c -o pw.exe && ./pw.exe
  (compiles with exit 0)
  fnv1a: f91dc7dd      # reference says 4c443831
  crc32: daafe952      # reference says f713df32

gcc -O2 -S checksum.c -o checksum.s && gcc checksum.s -o roundtrip.exe
  && ./roundtrip.exe
  fnv1a: 4c443831      # byte-round-trip gate passes
  crc32: f713df32

gcc -O1 -fno-inline -c checksum.c -o o1.o && objdump -d o1.o
  fnv1a at O1: length-check je prologue, %r8 pointer loop, separate epilogue
gcc -O2 -fno-inline -c checksum.c -o o2.o && objdump -d o2.o
  fnv1a at O2: mov $0x811c9dc5, movzbl+xor, imul $0x1000193, lea-bound loop
  (both O1 and O2 objects run to the same hashes)

objdump -d -j .rdata checksum.exe | head
  "fnv1a: %08x\n" decodes as:
    66 6e          data16 outsb (%rsi),(%dx)
    76 31          jbe ...
    61             (bad)
    25 30 38 78 0a and $0xa783830,%eax     <- a format string, not code
```

## Verified facts (runtime, all exit 0)

- fnv1a/crc32 hashes above reproduce identically across runs.
- The `-O2` disassembly contains the FNV prime as `imul $0x1000193` and the
  CRC polynomial as `and $0xedb88320`; a reconstruction changing either
  constant is caught by the gate.
- `movzbl (%rcx),%edx` (zero-extension) is present in both hashes; feeding
  `0x80`/`0xFF` bytes makes the signed-byte defect in `plausible_wrong.c`
  diverge (rule 7).
- objdump decodes `.rdata` strings into bogus instructions (rule 6) — confirmed
  by `-j .rdata` output above.

## Scoring (for routing eval)

- precision: every flagged "bug" maps to a named reference rule (1-8) and is
  confirmed by the behavioral gate.
- recall: plausible_wrong's two defects, wrong constants, precedence errors,
  and signed-byte errors are all caught by running the reconstruction.
- FP-rate: correct source, verified round-trips, and data-as-code artifacts
  produce zero flags.

## Target toolchains (absent, documented)

- A real decompiler (Ghidra/IDA) and its round-trip: not installed; the
  objdump-level gates above are the executable subset available here.
- `capstone` (to re-verify objdump decodes independently): not installed —
  `capstone-docs` documents the same API for the target machine.
