# Evaluation — meta-verification-harness-validity

Skill: `skills/_meta/meta-verification-harness-validity`. Stability target:
`evaluated`. Toolchain: gcc 16.1.0 (MSYS2, Windows x86_64), recorded
2026-08-15. SOURCE-BACKED — every example below was compiled and run, exit
codes captured.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/harness_masks_bug.c` | harness returns 0 while target broken | exit 0 — MASKED |
| medium/negative | `bad/harness_no_execute_path.c` | assertion gated behind never-set flag | exit 0 — MASKED |
| medium/positive | `good/ablation_delta.c` (correct target) | valid harness passes | exit 0 |
| hard/positive | `good/ablation_delta.c` `-DBROKEN_TARGET` | same harness FAILS on broken target | exit 0xC0000409 (abort) |
| hard/positive | `good/harness_coverage_gate.c` | branch counters asserted | exit 0 |
| hard/positive | `good/harness_real_pass.c` | genuinely correct harness passes | exit 0 |

Detection rule: for any claimed PASS, demand the ablation run — the same
harness against a broken target must fail. The two bad fixtures pass while
their targets are broken, which is the disqualifying property.

## False-positive evals (valid harnesses must NOT be flagged)

- `good/ablation_delta.c` without the flag — asserts are real, the harness
  is proven target-sensitive by its own `-DBROKEN_TARGET` run.
- `good/harness_coverage_gate.c` — all three branches of the target are
  reached AND asserted via counters; removing any call fails its assert.
- `good/harness_real_pass.c` — correct target, edge-case asserts (including
  the overflow path); passes and must not be "improved into failure".
- A harness whose single assert is tight but complete (one input that fully
  exercises the predicate) is valid — do not require N asserts mechanically.

## Historical evals

- claude-code#82057 (2026-07-28): three "passing" repaint harnesses masked
  a rendering regression; the passing gate was green because the defect path
  was never observed. KNOWN (documented in
  `research/2026-08-15-agent-failures-survey.md`, RST-5); reproduced here by
  `bad/harness_masks_bug.c` (the "always repaint, always pass" shape).
- MintVID (2026): hand-written asm passed review but never executed until
  `qemu -d exec` proved the path dead. The never-executed-check shape is
  reproduced by `bad/harness_no_execute_path.c`; `qemu -d exec` is the
  documented on-machine oracle (qemu-docs) — UNVERIFIED on this host (QEMU
  not installed).
- arxiv-2606-20128 (Correctness Illusion): fixed-shape allclose oracle
  certified buggy GPU kernels; fuzz + fp64 caught 9/9. KNOWN abstract
  figures; the oracle-masking shape is reproduced locally.

## Adversarial evals

- `bad/harness_masks_bug.c` compiles, runs, and exits 0 while the target
  never clamps — an agent that stops at "exit 0" declares PASS and misses
  the defect. The ablation-delta run is the only thing that exposes it.
- `bad/harness_no_execute_path.c` looks like it has a check (an assert
  exists in source) but the check is unreachable in the default invocation —
  reading the code is not enough; the invocation must be traced.
- The flip side: `good/ablation_delta.c -DBROKEN_TARGET` is a harness that
  legitimately FAILS. An agent over-eager to "fix" a failing test must not
  relax the assert to make the broken target pass — that is exactly the
  repaint-masks-bug failure mode.

## Verification commands (ACTUAL, recorded 2026-08-15, gcc 16.1.0)

```
gcc examples/bad/harness_masks_bug.c -o m.exe && ./m.exe
  prints "harness says PASS regardless of target behavior"   exit 0

gcc examples/bad/harness_no_execute_path.c -o n.exe && ./n.exe
  prints "checksum harness reported PASS"                     exit 0

gcc examples/good/ablation_delta.c -o ok.exe && ./ok.exe
  prints "harness PASS"                                       exit 0

gcc examples/good/ablation_delta.c -DBROKEN_TARGET -o br.exe && ./br.exe
  prints "Assertion failed: bounded_value(150) == 100, ... line 28"
  exit 0xC0000409 (abort, STATUS)

gcc examples/good/harness_coverage_gate.c -o c.exe && ./c.exe
  prints "harness PASS: full branch coverage achieved"        exit 0

gcc examples/good/harness_real_pass.c -o r.exe && ./r.exe
  prints "harness PASS: checked_add holds for edge cases"     exit 0
```

## Scoring

- precision: every flagged harness must have a demonstrable masking path
  (unconditional return, unreachable check, no comparator).
- recall: the ablation-delta must be demanded before any PASS is accepted.
- FP-rate: the three good fixtures (incl. the abort-on-broken-target run)
  produce zero flags.
- Strongest single fact: the SAME `ablation_delta.c` exits 0 with a correct
  target and 0xC0000409 with `-DBROKEN_TARGET` — target-sensitivity is
  recorded, not assumed.
