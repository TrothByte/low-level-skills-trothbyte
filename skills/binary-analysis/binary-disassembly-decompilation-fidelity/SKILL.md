---
name: binary-disassembly-decompilation-fidelity
description: Use when decompiling or judging decompiled code from x86-64 binaries, or when a reconstructed C function "looks right". Apply re-executability and byte-round-trip gates because decompilation is plausible-but-wrong (DeGPT 37% CFR, SCDBench 7%, Meta LLM Compiler 14% exact, cliff at ~200 instructions).
---

# Binary Analysis: Decompilation Fidelity

## When to use

- Decompiling a function (Ghidra/IDA/objdump) and writing the "equivalent" C.
- Reviewing decompiled output before trusting it or patching from it.
- Deciding whether a round-trip succeeded: disassembly → code → binary → behavior.
- Reasoning about why a model's "clean-looking" reconstruction diverges at runtime.

## When not to use

- Reading disassembly purely to understand register/addressing mechanics — use
  `asm-x86-64-registers-and-addressing`.
- Recovering types from instruction-width patterns — use
  `binary-analysis-type-recovery`.
- Reading compiler-optimizer artifacts (`-O2` shapes, tail calls) — use
  `asm-optimizer-artifacts`.
- Analyzing shellcode bytes — use `reverse-engineering-shellcode-analysis`.

## What the agent often gets wrong

- Treating "compiles cleanly" as "is correct": a plausible reconstruction
  assembles and runs but diverges on edge inputs (recorded: both outputs wrong
  in `examples/bad/plausible_wrong.c`).
- Trusting a single disassembly as ground truth: the same source at `-O1` vs
  `-O2` produces different instruction streams with identical behavior.
- Treating `movzbl`/`movslq` as the *source type*: the disassembly cannot tell
  you signedness or aliasing; two different C sources compile to the same bytes.
- Believing "clean" objdump output: objdump decodes ANY bytes — `.rdata`
  strings disassemble into plausible instructions (`data16 outsb`, `jbe`,
  `(bad)`, `and $0xa783830,%eax`). Clean output is not evidence.
- Reporting compile@k as success: code that compiles is not code that passes
  (DEC-8: compile@k5 up to 79.4%, pass@k much lower).
- Assuming decompilation improves with model size (DEC-1..13 show exact-match
  still ~14% and a capability cliff near 200 instructions).

## How to reason correctly

1. Frame every claim as a *behavioral* claim: "the reconstruction reproduces
   the binary's I/O for tested inputs", never "the reconstruction is what the
   source was".
2. Run the re-executability gate: compile your reconstructed C, run it against
   the same inputs as the reference binary, compare outputs. Mismatch → reject.
3. Run the byte-round-trip gate: reassemble the exact disassembly (or
   recompile the reference source) and confirm behavior is preserved; only the
   encoding, not the source, is recoverable.
4. Enumerate what the disassembly cannot encode: signedness, aliasing, original
   loop bounds, variable lifetimes, field types (only widths are visible).
5. When a reconstruction "passes", state the tested input set; expanding inputs
   is the next eval step (SCDBench: code looks plausible and compiles but
   semantics diverge).
6. For large functions (>~200 instructions), assume decompilation degrades;
   require execution-based verification, not reading.
7. Never use a disassembler's output as proof that a memory region is code;
   section boundaries, not clean output, decide that (FP rule).

## What to verify

- Reference binary behavior on a fixed input set (recorded).
- Reconstructed C compiles with `-Wall -Wextra -Werror` AND produces identical
  output on the same input set.
- The exact disassembly reassembles to behaviorally identical code.
- `objdump` output is labeled with its source section; `.rdata`/`.data` bytes
  are not reported as code.
- Claims about signedness/aliasing are marked INFERRED unless proven.

## How to verify

```
gcc -O2 checksum.c -o reference.exe && ./reference.exe   # record outputs
objdump -d reference.exe                                   # record disassembly
gcc -O2 -S checksum.c -o roundtrip.s                      # byte-round-trip gate
gcc roundtrip.s -o roundtrip.exe && ./roundtrip.exe       # must match reference
gcc -O2 plausible_wrong.c -o pw.exe && ./pw.exe           # plausible but wrong
objdump -d -j .rdata reference.exe                        # data-as-code FP demo
```

Recorded results for the fixture: `evals/README.md` (reference `fnv1a:
4c443831`, `crc32: f713df32`; plausible_wrong `f91dc7dd` / `daafe952`).

## Where the knowledge comes from

- `arxiv-2407-02524` — Meta LLM Compiler: asm→IR 45% round-trip, 14% exact match.
- `arxiv-2403-05286` — LLM4Decompile: re-executability as the metric; fine-tuned
  V2 64.9%.
- `arxiv-2510-19615` — DeGPT: "corrected fix rate" 37% — ~63% of "fixed" lines
  stay wrong.
- `arxiv-2605-29059` — SCDBench: ideal decompilation 7% (42/600); plausible,
  compiling, but wrong semantics.
- `arxiv-2607-06125` — capability cliff ~200 instructions; compile@k vs pass@k
  divergence.
- `arxiv-2510-20975` — REx86: base LLMs hallucinate x86 disassembly comments.
- `binutils-docs` — objdump `-d`, `-j`, section semantics.
- Empirical: GCC 16.1 / objdump 2.46 (MSYS2), recorded 2026-08-15.

## Related skills

- `binary-analysis-type-recovery` — what instruction widths do (and do not) reveal
- `reverse-engineering-shellcode-analysis` — byte-accurate reading of raw code
- `asm-optimizer-artifacts` — how `-O2` shapes the disassembly you are reading
- `concurrency-actual-parallelism-detection` — same "fake vs real" gate applied
  to thread claims
- `binary-memory-leak-vm-allocator-diagnosis` — behavioral diagnosis over
  plausibility

## Evaluation

Synthetic: recover `fnv1a`/`crc32` behavior from the recorded disassembly;
any reconstruction must reproduce `4c443831`/`f713df32` for the fixture input,
and must fail on `plausible_wrong.c` (which compiles cleanly but yields
`f91dc7dd`/`daafe952`).
False-positive: do NOT flag the correct source, a recompiled identical binary,
or a legitimate `movzbl` read; do NOT treat `.rdata` disassembly as code.
Historical: DeGPT 37% CFR and SCDBench 7% calibrate the prior — assume
decompilation is wrong until gated.
Adversarial: the disassembly contains `imul $0x1000193` and `and $0xedb88320`
immediates; a reconstruction that writes a different constant or wrong
precedence must be rejected by the behavioral gate.
Commands and verified facts: `evals/README.md`.
