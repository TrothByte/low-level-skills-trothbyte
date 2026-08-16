---
name: accelerator-pipeline-synchronization
description: Use when writing or reviewing AI-accelerator pipeline programs (DMA, vector, matrix, scalar units on shared on-chip buffers): checking barrier/sync coverage across units, because missing or misplaced synchronization escapes simulation and golden testing. Teaches happens-before reasoning over cross-unit write-read pairs.
---

# Accelerator Pipeline Synchronization

## When to use

- Writing or reviewing a multi-stage accelerator program where DMA, vector,
  matrix, and scalar units run concurrently on shared on-chip buffers.
- Debugging nondeterministic outputs that pass golden tests on one hardware
  configuration and fail on another.
- Reviewing LLM-generated accelerator kernels: 19.2% of 120 such kernels had
  a synchronization defect.
- Choosing where vendor pipe barriers / sync primitives must go, or auditing a
  program that "works in simulation" but corrupts data on hardware.

## When not to use

- GPU/CUDA kernels — use `gpu-memory-model-coherence` and
  `gpu-kernel-verification-beyond-oracle`.
- CPU multithreading — use `memory-ordering-reasoning` /
  `atomics-c11-cpp11-rust`.
- HDL CDC (clock domain crossing) in FPGA logic — use `hdl-cdc-audit`.

## What the agent often gets wrong

- "It passed simulation and the golden test, so the sync is fine." Cross-unit
  data races escape BOTH: neither simulation nor golden testing models the
  accelerator's cross-unit visibility semantics. A single golden run samples
  one interleaving and can be lucky.
- "There is a barrier in the program, so the pair is safe." A misplaced
  barrier — after the read it was supposed to guard, or covering the wrong
  dependency — orders nothing it needs to.
- "Barriers are expensive, so one at the end is enough." A barrier must sit
  BETWEEN the cross-unit write and the read of the same buffer; one barrier
  per stage boundary does not cover all write-read pairs on shared buffers.
- "Only vector/matrix data needs sync." DMA loads and stores are also units;
  a store that races a producer corrupts output just like a load racing a
  producer.
- "The hardware sanitizer will catch it." AccelSync detects hazards that
  Huawei's runtime sanitizer (msSanitizer) misses, at 400x lower cost per
  kernel — and 3 previously unknown hazards were found in a production kernel
  library (CANN).

## How to reason correctly

1. Model the pipeline as a program of unit instructions (unit, op, buffer)
   and barriers. Units execute concurrently; program order only holds within
   one unit.
2. Enumerate every cross-unit write-read pair on the same buffer. Each such
   pair is a potential race: the read can see stale data.
3. For each pair, ask: does a barrier sit strictly BETWEEN the write and the
   read? If yes, the pair is ordered (happens-before). If no, it is a hazard
   regardless of how plausible the scheduling looks.
4. Check same-unit pairs too: program order within a unit means the write
   must precede the read; a reversed order is a hazard no barrier can fix.
5. Verify barrier sufficiency, not barrier existence: coverage is the
   correctness property (decidable, O(|E|^2) under the AccelSync model).
6. Treat a passing golden run as one sampled interleaving, never as proof:
   enumerate or reason about the interleavings the sync allows, and require
   the hazard-free property, not one lucky schedule.

## What to verify

- Every cross-unit write-read pair on the same buffer has a barrier between.
- Every barrier is positioned strictly between its write and its read.
- Same-unit pairs are in program order (write before read).
- DMA loads/stores are units in the model, not afterthoughts.
- A sync "exists somewhere" is not a verdict; coverage is.

## How to verify

```
# Python pipeline model + barrier-sufficiency checker (plain python 3.11):
python examples/good/good_sync_schedule.py      # expected: SAFE, 0 hazards
python examples/bad/bad_missing_sync.py         # expected: UNSAFE, 2 hazards
python examples/bad/bad_wrong_sync_order.py     # expected: UNSAFE, 2 hazards

# Target (accelerator hardware or vendor toolchain; documented-as-target):
# Ascend: CANN operator pipeline + pipe barriers (aclnn / ascendc)
# Cambricon: MLU370 pipeline sync primitives
# AccelSync-style check: enumerate cross-unit write-read pairs on each buffer
# and confirm barrier coverage before deploying.
```

## Where the knowledge comes from

- `arxiv-2605-07881` — AccelSync: missing/misplaced sync introduces
  hardware-visible races that escape simulation and golden testing; barrier
  sufficiency is decidable in O(|E|^2); 19.2% defect rate (95% CI
  [13.0%, 27.4%]) on 120 LLM-generated kernels; 3 unknown hazards in 6,292
  CANN kernels; beats msSanitizer at 400x lower cost. (new source, proposed)
- `cuda-cpp-guide` / `ptx-isa` — the sync-primitive vocabulary (bar.sync,
  __syncthreads) GPUs use, for cross-referencing the abstract barrier model.
- `gpu-memory-model-coherence` — the sister problem on GPUs: cross-thread
  visibility without __syncthreads().

## Related skills

- `memory-ordering-reasoning` (recommend) — happens-before reasoning is the
  same tool; barriers on accelerators are sync points, not memory fences.
- `hdl-cdc-audit` (recommend) — cross-unit pipeline sync is the hardware
  cousin of CDC; both need structural coverage checks, not one lucky test.
- `gpu-memory-model-coherence` (recommend) — GPU threads have the same
  "missing sync between producers and consumers" failure class.
- `meta-verification-harness-validity` (recommend) — a golden test is a
  harness; a harness that cannot observe the race is invalid for certifying
  the pipeline.

## Evaluation

Synthetic: `bad/bad_missing_sync.py` (no barrier between DMA->VEC and
VEC->SCL) and `bad/bad_wrong_sync_order.py` (barrier after the read it should
guard) must be flagged by the checker; `good/good_sync_schedule.py` (barrier
after every cross-unit write) must be SAFE.
False-positive: a program with redundant but harmless extra barriers must NOT
be flagged; a correct single-unit pipeline (no cross-unit pairs) must be SAFE.
Historical: AccelSync's audit data is the incidence record — 19.2% of
LLM-generated kernels, 3 unknown hazards in the CANN library, and the
nondeterministic-output observation on Ascend 910B2.
Adversarial: a barrier placed at the stage end instead of between a specific
write-read pair; a sync that covers the pair but uses the wrong ordering
direction; a golden test that passes once must be rejected as evidence.
Recorded output: `evals/README.md` (Python model actually executed on this
host; vendor hardware toolchain absent).
