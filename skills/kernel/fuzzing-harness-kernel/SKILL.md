---
name: fuzzing-harness-kernel
description: Use when building syzkaller descriptions, kernel fuzz harnesses, or coverage-guided kernel fuzzing. Teaches syscall-program generation, KCOV coverage feedback, harness initialization and seed corpora, crash minimization, and reproducing/classifying reports.
---

# Kernel Fuzzing Harnesses (syzkaller, KCOV, sanitizer feedback)

## When to use

- Writing syzkaller syscall descriptions (syzlang) for a new driver or
  subsystem, or setting up syzkaller against a kernel.
- Building a libFuzzer/AFL++ harness for a kernel-adjacent target (file
  systems, parsers, ioctl emulation) where KCOV or sanitizer coverage is
  the feedback channel.
- Triaging a fuzzer crash: reproducing it, minimizing the reproducer, and
  deciding whether it is a real bug or a harness bug.
- Setting up CI fuzzing with KASAN/KCSAN/UBSAN reports as the verdict.

## When not to use

- Fuzzing user-space libraries — use `libfuzzer-docs`/`aflpp-docs` directly
  (same shape, no kernel specifics).
- Judging whether a harness is trustworthy — use
  `fuzzing-harness-evidence-gate` and `meta-verification-harness-validity`.
- Exploitability assessment of the bugs found — use
  `kernel-exploitation-primitives`.
- Race hunting specifically — use `data-race-kernel-detection`.

## What the agent often gets wrong

- Writes syzkaller descriptions that never set up the driver's
  preconditions (open the device, bind the socket, mount the filesystem),
  so the fuzzer only exercises error paths. A syscall description without
  setup calls fuzzes nothing.
- Ignores the coverage feedback loop. Syzkaller and libFuzzer are
  coverage-guided: without KCOV (or sanitizer-coverage) the fuzzer is
  blind random testing that misses deep bugs — the classic "ran for 24h,
  no crashes" illusion (B2).
- Reuses one harness entry function for a stateful target without resetting
  state. `LLVMFuzzerTestOneInput` runs many inputs in one process; a
  stateful parser must reset (or reinit) its state per input or inputs
  contaminate each other.
- Handles fuzzer inputs as raw bytes with no length sanity: an unchecked
  `input[0]` when `size == 0` is a harness crash (a false positive that
  wastes a triage cycle and can mask real bugs).
- Assumes a reproducer from `syz-manager` runs standalone. Real kernel
  reproducers need the exact config (KASAN, KCSAN, KCOV), the boot
  parameters, and sometimes the exact VM image; `syz-reproduce` and
  `syz-minimize` are part of the loop.
- Reports a crash without classifying it: a KASAN report (real UAF) vs a
  KCSAN data race vs a UBSAN shift vs a hung task need different triage.
  "It crashed" is not a finding.
- Treats a crash on the *harness* itself (e.g. harness OOB on `size==0`) as
  a target bug. Harness bugs must be fixed first, or every later report is
  suspect.
- Forgets that corpus seeding is what makes coverage-guided fuzzing work:
  an empty seed corpus means the fuzzer rediscovers trivial coverage from
  scratch.

## How to reason correctly

1. State the target: which syscalls/ioctls/operations the harness must
   reach, and what state (device, socket, mount) each requires.
2. Build the setup path first: the harness/syzlang must perform the
   preconditions (open, bind, mount, `ioctl` init) before the fuzzable
   operations.
3. Wire the feedback loop: enable KCOV (`CONFIG_KCOV=y`) and the sanitizers
   (KASAN/KCSAN/UBSAN) and confirm the fuzzer actually consumes coverage
   (dashboard shows coverage growth, not just exec/s).
4. Make `LLVMFuzzerTestOneInput` stateless per input: guard `size`/`len`,
   reset globals, and free resources; add a self-test input set that
   exercises each reachable path.
5. On a crash: reproduce with `syz-reproduce`, minimize with
   `syz-minimize`, classify the report (KASAN/KCSAN/UBSAN/hang), then
   determine whether the fault is in the harness or the target.
6. Seed and evolve the corpus: start from real device captures / protocol
   dumps; let the fuzzer's own new inputs join the corpus.
7. Validate the harness with an ablation: inject a known bug and require
   the harness to find it (meta-verification-harness-validity).

## What to verify

- The fuzzer reaches the intended target path: a coverage dump shows the
  target functions/BBs, not just the setup code.
- The harness is stateless per input (guards on size, full reset) and its
  self-test corpus passes.
- Syzkaller descriptions include setup syscalls and the target's argument
  shapes (resource types, flags) — descriptions compile (`syz-sysgen`).
- KCOV/sanitizer coverage is actually enabled in the build and reported in
  the dashboard.
- Every crash has a minimized reproducer and a classified report; harness
  crashes are fixed before target crashes are trusted.
- The ablation check: a deliberately injected bug is found by the harness.

## How to verify

Host-side (coverage-guided fuzzing logic; no kernel on this host):

```
python examples/good/coverage_fuzzer.py
python examples/bad/naive_fuzzer.py
gcc -Wall -Wextra -Werror -O2 -c examples/good/LLVMFuzzer_harness.c
gcc -Wall -Wextra -Werror -O2 -c examples/bad/LLVMFuzzer_harness_bad.c
```

Target syzkaller (RESEARCHED; Linux+QEMU+syzkaller required, not here):

```
# build kernel with KCOV + KASAN
scripts/config -e KCOV -e KCOV_ENABLE_COMPARISONS -e KASAN -e DEBUG_INFO
make -j$(nproc)
# run syzkaller manager against QEMU, then per-crash:
syz-manager -config config.json
syz-reproduce -config config.json -prog <crash.prog>
syz-minimize -config config.json -prog repro.prog
```

## Where the knowledge comes from

- `syzkaller-docs` — setup, usage, internals, syzlang descriptions,
  repro/minimize, found-bugs lists
- `kernel-source` — KCOV implementation and Kconfig
- `libfuzzer-docs` — LLVMFuzzerTestOneInput contract, corpora, coverage
- `aflpp-docs` — coverage-guided fuzzing modes
- `oss-fuzz` — CI fuzzing, reproduction and disclosure process
- `fuzzing-harness-evidence-gate` — the gate that decides whether a
  fuzz finding is evidence
- `meta-verification-harness-validity` — ablation-delta for the harness

## Related skills

- `fuzzing-harness-evidence-gate` (require) — whether a fuzz run is
  evidence of anything
- `data-race-kernel-detection` (recommend) — triaging KCSAN findings
- `kernel-exploitation-primitives` (recommend) — assessing what a found
  crash is worth
- `sanitizer-report-reading` (recommend) — reading KASAN/UBSAN output
- `kernel-debugging-ftrace-kprobes-kdump` (recommend) — confirming
  reachability of the target path

## Evaluation

- Synthetic: a syzlang description with no setup calls, a stateless-input
  violation (size==0 deref), a naive no-coverage fuzzer, and a harness
  crash misreported as a target bug — each must be detected and fixed.
- False-positive: `good/coverage_fuzzer.py` (finds the injected bug),
  `good/LLVMFuzzer_harness.c` (state reset + size guard) must not be
  flagged.
- Historical: syzkaller's found-bugs lists (KASAN UAFs in drivers) and the
  OSS-Fuzz reports — the coverage-vs-random delta is reproduced by the two
  Python fixtures.
- Adversarial: `bad/naive_fuzzer.py` runs 1M random inputs and reports "no
  crashes" while the bug is reachable only via a specific depth-3 structure
  — coverage is the discriminator; `bad/LLVMFuzzer_harness_bad.c` compiles
  cleanly but is structurally wrong.
- Commands recorded on this host: `evals/README.md`.
