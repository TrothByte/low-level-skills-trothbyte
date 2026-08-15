---
name: meta-verification-harness-validity
description: Use before trusting a "passing" test harness, eval, or CI gate. Verifies the verification: a harness that cannot fail when its target is broken (unconditional pass, never-executed path, self-test bypass) certifies nothing. Teaches ablation-delta, coverage gates, and --self-test.
---

# Meta: Verify the Verification (Harness Validity)

## When to use

- A harness, test suite, eval, or CI job reports PASS and you are about to
  conclude "the code is correct".
- An agent wrote the harness itself and claims the result proves a target
  (the harness author is the least reliable person to certify it).
- Reviewing a fix where the regression test "passes" but you cannot explain
  what condition would have failed before the fix.
- Choosing between a harness that "runs and prints OK" and one that actually
  compares outputs (e.g. a decompiler/emulator/RE eval).

## When not to use

- The harness is already known to fail on a deliberately broken target and
  that run is recorded — the ablation-delta is already established.
- Pure documentation work with no executable claim.
- The goal is writing the target code, not certifying a test — use the
  target-domain skill (e.g. `c-undefined-behavior`).

## What the agent often gets wrong

- Treats "harness ran and exited 0" as evidence about the target. It is only
  evidence about the harness: `bad/harness_masks_bug.c` returns 0 while the
  target under test never clamps anything — recorded exit 0 (masked defect).
- Writes harnesses where the interesting path is skipped by default:
  `bad/harness_no_execute_path.c` gates its only assertion behind an
  environment flag the CI never sets — recorded exit 0.
- Never performs the ablation check: a harness is valid only if it FAILS
  when the target is broken. `good/ablation_delta.c` compiled with
  `-DBROKEN_TARGET` aborts on its first assert (recorded exit 0xC0000409);
  compiled without the flag it passes (exit 0). Same harness, same asserts —
  only the target differs.
- Mistakes "runs" for "tests". An eval that prints a repaint, a hash, or an
  "OK" line without a comparator exercises the harness, not the target.
- Fixes the harness to pass instead of fixing the target (repaint-masks-bug
  regression debugging, claude-code#82057).
- Writes coverage-gated harnesses that measure line execution but not branch
  semantics — a broken branch that is still *reached* passes.
- Accepts a "green" number produced by hiding the violation (see
  `hdl-timing-closure`): making a gate pass is not the same as satisfying it.

## How to reason correctly

1. **Name the predicate**: write down the exact condition the harness checks
   ("clamp returns x in [0,100]", "checksum == 6"). If you cannot state it,
   the harness has none.
2. **Ablation-delta**: break the target (or run a known-broken variant) and
   require the harness to FAIL. If it still passes, it is not testing the
   target. This is the single decisive test of harness validity.
3. **Coverage gate**: every branch/region of the target that contributes to
   the predicate must be reachable by the harness, and the harness must
   assert reachability (counters per region, as in
   `good/harness_coverage_gate.c`). Line coverage of a never-executed path
   is worthless.
4. **Self-test**: add a `--self-test` mode that runs a known-good and a
   known-bad input and asserts the harness distinguishes them.
5. **Adversarial target**: once the harness is validated, run it against an
   intentionally wrong target in the same shape as the real one (e.g.
   `-DBROKEN_TARGET`), not only against the shipped target.
6. Mark any claim certified only by an unvalidated harness as INFERRED.

## What to verify

- The harness FAILS when the target is broken (real run, recorded exit
  code, not assumed).
- Every branch the harness claims to cover is actually reached and the
  harness asserts that reachability.
- No exit-0 path exists that bypasses all assertions (no flag-gated
  self-checks, no unconditional `return 0`).
- For a decompiler/emulator/RE harness: the harness output is compared to a
  ground truth oracle, not printed and eyeballed.
- The recorded PASS is reproducible with the exact commands in the evals
  README.

## How to verify

```
gcc examples/good/ablation_delta.c -o ok.exe && ./ok.exe
  exit 0                      # correct target, harness passes

gcc examples/good/ablation_delta.c -DBROKEN_TARGET -o br.exe && ./br.exe
  exit 0xC0000409 (abort)     # same harness must FAIL on broken target

gcc examples/bad/harness_masks_bug.c -o m.exe && ./m.exe
  exit 0                      # unconditional-pass harness certifies nothing
gcc examples/bad/harness_no_execute_path.c -o n.exe && ./n.exe
  exit 0                      # assertion path never taken -> void gate

gcc examples/good/harness_coverage_gate.c -o c.exe && ./c.exe
  exit 0                      # branch counters asserted -> valid gate
gcc examples/good/harness_real_pass.c -o r.exe && ./r.exe
  exit 0                      # genuinely correct harness must NOT be flagged
```

## Where the knowledge comes from

- `arxiv-2606-20128` (Correctness Illusion): fixed-shape allclose oracles
  certify buggy GPU kernels — the oracle does not test what it claims.
- `arxiv-2607-00107` (Illusion of Safety): verification that only looks
  rigorous overstates guarantees for AI-written code.
- `binutils-docs` (objdump/readelf) — independent disassembly as the
  comparison oracle when validating an asm-level harness.
- `qemu-docs` (`-d exec`) and MintVID: asm that never executed passed
  review until `qemu -d exec` proved the path was dead.
- claude-code#82057: three "passing" repaint harnesses masked the real
  regression bug — documented in `research/2026-08-15-agent-failures-survey.md`.

## Related skills

- `meta-verification` — chooses and records gates; this skill validates
  that the gate itself is sound.
- `meta-evidence`, `meta-completion` — a claim is not KNOWN until the
  harness that produced it is shown to be target-sensitive.
- `meta-rationalizations` — "the tests pass" is the classic rationalization.
- `sanitizer-agent-ci-loop` — CI gates that pass for the wrong reason.
- `hdl-timing-closure` — "green timing by hiding the violation" is the
  hardware twin of this problem.

## Evaluation

- Synthetic: `bad/harness_masks_bug.c` and `bad/harness_no_execute_path.c`
  must be rejected (they pass while the target is broken); the ablation
  check must be demanded.
- False-positive: `good/ablation_delta.c` (without the flag),
  `good/harness_coverage_gate.c`, and `good/harness_real_pass.c` are
  genuinely valid and must NOT be flagged.
- Historical: claude-code#82057 (three passing repaint harnesses masked a
  regression) and MintVID (asm never executed until `qemu -d exec`).
- Adversarial: a harness that passes on the broken target because its
  assertion is flag-gated or its result is discarded — both reproduced.
- Commands and recorded outputs (gcc 16.1.0, Windows): `evals/README.md`.
