# Evaluation — accelerator-pipeline-synchronization

Skill: `skills/accelerator/accelerator-pipeline-synchronization`.
Stability: `researched` (source-backed grounding: arxiv-2605-07881 — abstract
fetched and verified 2026-08-17). Vendor accelerator toolchain (Ascend/CANN,
Cambricon, any NPU/TPU) is NOT available on this host; the barrier-sufficiency
model and checker were executed here as a Python simulation. Mark: SIMULATED —
models the AccelSync ordering semantics, not a specific accelerator.

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| negative (missing) | `bad/bad_missing_sync.py` | 2 cross-unit hazards (DMA->VEC buf0, VEC->SCL buf1) | RUN on host |
| negative (misplaced) | `bad/bad_wrong_sync_order.py` | 2 hazards; barriers exist but none strictly between pair | RUN on host |
| positive | `good/good_sync_schedule.py` | SAFE, 0 hazards | RUN on host |

Detection rule: a pipeline is correct only if every cross-unit write-read pair
on the same buffer has a barrier strictly between write and read (barrier
sufficiency, decidable in O(|E|^2) under the AccelSync semantics).

## False-positive evals (correct code must NOT be flagged)

- `good/good_sync_schedule.py` — barriers after every producer, all pairs
  covered: must be SAFE.
- A single-unit pipeline with no cross-unit pairs must be SAFE (no pairs to
  cover).
- Redundant extra barriers that do not hurt ordering must NOT be flagged.

## Historical evals

- AccelSync audit (arxiv-2605-07881): 19.2% defect rate (95% CI [13.0%,
  27.4%]) on 120 LLM-generated kernels; 3 previously unknown hazards in 6,292
  production CANN kernels; a hazard class produced nondeterministic outputs on
  Ascend 910B2 under CANN 8.0.RC3; msSanitizer missed hazards AccelSync finds,
  at 400x lower cost per kernel. No CVE is attributed here; the incidence
  record is the empirical basis for this skill.

## Adversarial evals

- A barrier at the end of a stage that is after the read it should guard must
  be caught (misplaced class, fixture 2).
- A barrier covering the wrong dependency (e.g., between SCL and MAT while the
  VEC->SCL pair is uncovered) must be caught by the per-pair check.
- A golden test that passes once must be rejected as evidence of sync
  correctness: the stale-read interleaving exists even when one run was lucky.
- A "fix" by timing (sleep/poll) instead of a barrier must be rejected as
  unverifiable (taxonomy rule 5).

## Verification commands

```
python examples/good/good_sync_schedule.py
python examples/bad/bad_missing_sync.py
python examples/bad/bad_wrong_sync_order.py
```

Target (accelerator hardware; documented-as-target, not executed here):

```
# Ascend / CANN: compile the operator and run under msSanitizer, then run the
# same pipeline under a second toolkit/driver configuration and diff outputs
# (nondeterminism across configurations is the race signature).
# The barrier-sufficiency checker above is the primary gate: run it before
# deploying any LLM-generated pipeline kernel.
```

## Verified facts

| Fact | Status | Evidence |
|---|---|---|
| python 3.11.9 runs all three pipeline fixtures; SAFE/UNSAFE verdicts match expectation | VERIFIED (executed 2026-08-17) | output below |
| missing-sync fixture: 2 hazards (DMA->VEC buf0, VEC->SCL buf1) | VERIFIED (executed) | output below |
| misplaced-sync fixture: 2 hazards with barriers present | VERIFIED (executed) | output below |
| barrier-sufficiency formulation, O(|E|^2) decidability, 19.2% LLM-kernel defect rate, CANN findings, msSanitizer comparison | KNOWN (abstract fetched) | arxiv-2605-07881 |
| races escape simulation and golden testing | KNOWN (abstract fetched) | arxiv-2605-07881 |
| behavior of this model on real Ascend/Cambricon hardware | UNVERIFIED | toolchain absent |

### Host run (python 3.11.9, executed 2026-08-17)

`python examples/good/good_sync_schedule.py`:

```
program: LayerNorm pipeline, barriers after every producer
  no hazards: every cross-unit write-read pair on the same
  buffer has a barrier strictly between write and read.
  verdict: SAFE (barrier-sufficient)
```

`python examples/bad/bad_missing_sync.py`:

```
program: LayerNorm pipeline, stage barriers MISSING
  HAZARD cross-unit buf0: write@0(DMA) -> read@1(VEC) -- no barrier between; a stale-read interleaving EXISTS (golden run can pass by luck)
  HAZARD cross-unit buf1: write@2(VEC) -> read@3(SCL) -- no barrier between; a stale-read interleaving EXISTS (golden run can pass by luck)
  verdict: UNSAFE (2 hazard(s))
```

`python examples/bad/bad_wrong_sync_order.py`:

```
program: LayerNorm pipeline, barriers MISPLACED
  HAZARD cross-unit buf0: write@0(DMA) -> read@1(VEC) -- barriers exist but none sits strictly between write and read; stale-read interleaving EXISTS
  HAZARD cross-unit buf1: write@3(VEC) -> read@4(SCL) -- barriers exist but none sits strictly between write and read; stale-read interleaving EXISTS
  verdict: UNSAFE (2 hazard(s))
```

Interpretation: the SAFE program and the two UNSAFE programs are
distinguishable only by per-pair barrier coverage; a golden run of the missing-
or misplaced-sync program can pass by luck, which is precisely the documented
escape from simulation and golden testing.

## Scoring (for routing eval)

- recall: both hazard classes (missing, misplaced) detected.
- precision: correctly synced pipeline produces zero flags.
- FP-rate: no false positives on the good schedule or redundant-barrier
  variants.
