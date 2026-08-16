# Evaluation — fuzzing-harness-kernel

Skill: `skills/kernel/fuzzing-harness-kernel`. Stability target: `evaluated`.
Current stability: `source-backed` for the host-side logic — the Python
fuzzer fixtures and C harness compiles were run on this host (gcc 16.1.0,
python 3.11.9) and outputs recorded. Syzkaller/KCOV target runs are
documented-as-target, NOT executed here.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/naive_fuzzer.py` | no coverage feedback, bug missed | "no crashes", exit 0 (MASKED) |
| easy/negative | `bad/LLVMFuzzer_harness_bad.c` | size==0 deref, no state reset | compiles clean (structural bug) |
| medium/negative | `bad/syzlang_descs_bad.txt` | ioctl without resource producer | description missing setup (sysgen UNVERIFIED) |
| medium/positive | `good/coverage_fuzzer.py` | coverage-guided loop finds bug | CRASH found, exit 0 |
| hard/positive | `good/LLVMFuzzer_harness.c` | size guard + per-input reset | compiles with -Werror |
| hard/positive | `good/syzlang_descs.txt` | resource chain open->ioctl | resource declared (sysgen UNVERIFIED) |

Detection rule: for every harness, demand (a) the feedback loop is real
(coverage consumed), (b) the entry is stateless and size-guarded, (c) setup
paths exist, and (d) the ablation (injected bug found) was run.

## False-positive evals (correct harnesses must NOT be flagged)

- `good/coverage_fuzzer.py`: coverage-gated corpus growth and a found bug —
  valid, no flag.
- `good/LLVMFuzzer_harness.c`: size guard, memset-reset, bounded loop — the
  required stateless shape, no flag.
- `good/syzlang_descs.txt`: resource chain present — no flag.

## Historical evals

- syzkaller's Linux found-bugs lists (KASAN UAFs, KCSAN races in drivers,
  fs bugs) — the coverage-vs-random delta is reproduced by the two Python
  fixtures. KNOWN abstract (syzkaller docs lists); specific reports
  UNVERIFIED on this host.
- OSS-Fuzz practice: harness bugs (size==0 derefs) must be separated from
  target bugs — reproduced by the LLVMFuzzer fixture pair. KNOWN (OSS-Fuzz
  docs).

## Adversarial evals

- `bad/naive_fuzzer.py` runs 1,000,000 inputs and reports "clean" while the
  target's bug is reachable — an agent that accepts "no crashes" without
  coverage evidence reproduces the illusion. The expected answer: the run
  proves nothing about the target.
- `bad/LLVMFuzzer_harness_bad.c` compiles with `-Werror` yet dereferences
  data[0] on size==0 — compile-clean ≠ harness-correct.
- `bad/syzlang_descs_bad.txt` looks like a complete description until the
  missing producer resource is noticed.

## Verification commands (host, ACTUAL)

```
python examples/good/coverage_fuzzer.py
  GOOD: coverage-guided fuzzer found the bug at iteration <k>,
  corpus size <m>                                           exit 0
python examples/bad/naive_fuzzer.py
  fuzzed 1000000 inputs, crashes=0
  RESULT: no crashes found -> target appears clean
  BAD: ... 'clean' is an artifact                           exit 0 (MASKED)
gcc -Wall -Wextra -Werror -O2 -c examples/good/LLVMFuzzer_harness.c
  exit 0
gcc -Wall -Wextra -Werror -O2 -c examples/bad/LLVMFuzzer_harness_bad.c
  exit 0 (compiles: the bug is structural, not syntactic)
```

## Verification commands (target, RESEARCHED — not run on this host)

```
# kernel with KCOV + KASAN + DEBUG_INFO
scripts/config -e KCOV -e KCOV_ENABLE_COMPARISONS -e KASAN -e DEBUG_INFO
make -j$(nproc)
# syzkaller against QEMU
syz-manager -config config.json
syz-reproduce -config config.json -prog <crash.prog>
syz-minimize -config config.json -prog repro.prog
# sysgen check for the description fixtures:
syz-sysgen
```

## Verified facts

- `good/coverage_fuzzer.py` found the injected bug within its budget and
  reported corpus growth (KNOWN, recorded).
- `bad/naive_fuzzer.py` ran 1M inputs with zero crashes against the same
  target (KNOWN, recorded) — the coverage delta is measured.
- Both C harnesses compile with `-Wall -Wextra -Werror -O2` (KNOWN).
- KCOV requirements and Kconfig options — KNOWN from kernel-source and the
  syzkaller docs (fetched 2026-08-17), cited to proposed source
  `syzkaller-docs`.
- LLVMFuzzerTestOneInput contract (stateless entry, size guard) — KNOWN
  from libfuzzer-docs.
- Actual syzkaller/syz-sysgen/KCOV runtime behavior on a real kernel —
  UNVERIFIED on this host.

## Scoring

- precision: a fuzz finding is evidence only when coverage is wired, the
  entry is stateless, setup paths exist, and the ablation found a planted
  bug.
- recall: coverage feedback, per-input reset, size guards, setup calls,
  repro/minimize, and report classification are each demanded.
- FP-rate: the three good fixtures produce zero flags.
- Strongest single fact: the same target is "clean" after 1M random inputs
  and "crashed" by the coverage-guided fuzzer — the feedback-loop delta is
  recorded, not assumed.
