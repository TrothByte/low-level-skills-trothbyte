---
name: hdl-cdc-audit
description: Use when reviewing or fixing clock domain crossing in HDL: classifying the crossing, choosing the right synchronization (2-FF for single-bit, gray-code/handshake/FIFO for multi-bit), and rejecting constraints that hide a CDC bug instead of fixing it. Prevents per-bit 2-FF on multi-bit buses and single-FF "synchronizers".
---

# HDL Clock Domain Crossing Audit

## When to use

- Reviewing RTL that crosses a signal, bus, pointer, or FIFO between two
  clock domains.
- Choosing a synchronization scheme for a new crossing (single-bit level,
  single-bit pulse, counter, multi-bit data bus).
- Reading a design where a CDC violation was "fixed" with a timing
  constraint or a `set_false_path`.
- Auditing an AI-generated module for CDC mistakes (the most common HDL
  review miss).

## When not to use

- Single-clock design with no domain boundary — there is no CDC to classify.
- Clock gating / clock switching logic — a different discipline (use
  `embedded-*` / FPGA tool docs for BUFGCTRL and glitch-free muxing).
- Full timing closure methodology — use `hdl-timing-closure`; constraints
  that are *correct exceptions* belong to `hdl-constraints-authoring`.
- Verilog syntax / lint triage generally — use `hdl-constraints-authoring`
  or the lint tool's own docs.

## What the agent often gets wrong

- Copies "2-FF synchronizer" onto every crossing, including multi-bit data
  buses, producing incoherent words (mixed old/new bits). The Python model
  `examples/bad/cdc_multibit_naive_sync.py` reproduces captured values
  (0b0000, 0b1000) that never existed on the source bus.
- Uses a single flip-flop for a CDC input and calls it a synchronizer;
  one stage is insufficient — two are the accepted minimum.
- Treats a `set_false_path` / `set_max_delay` as the fix for an
  unsynchronized crossing. A constraint changes the timing report, not the
  sampled value: the destination flop still goes metastable and the data
  can still be wrong.
- Synchronizes a pulse with a 2-FF level synchronizer: a short source
  pulse can be missed entirely in the destination domain.
- Forgets classification is the first step (Cummings): classify the signal
  type, then pick the mechanism. Jumping to a mechanism without the
  classification is the root error.
- Sends a binary counter across a boundary and then complains the gray-code
  conversion is "extra work" — the gray code is the thing that makes the
  crossing safe when only one bit changes per step.

## How to reason correctly

1. **Classify the signal** (Cliff Cummings method):
   - single-bit level → 2-FF synchronizer
   - single-bit pulse → pulse synchronizer (toggle + 2-FF + edge detect)
   - counter/pointer that only changes by 1 → gray-code encoding + 2-FF per bit
   - arbitrary multi-bit data with flow control → handshake or async FIFO
     with gray-coded pointers
2. **For any multi-bit crossing**, ask: "can the destination observe a
   mixed old/new word?" If yes, per-bit synchronization is unsafe regardless
   of how many flops each bit gets. Only gray-code (one bit flips per
   transition) or a FIFO/handshake eliminates the mixing.
3. **Check the constraint** does not replace a synchronizer. Verify the RTL
   has an actual synchronization stage on every async input; a clean timing
   report is necessary but not sufficient.
4. **Simulate the crossing** with a CDC-aware model (see the Python models)
   before sign-off: change the source bus and check that no intermediate
   destination word appears that never existed at the source.
5. Name the evidence per crossing in the review output: mechanism chosen,
   why it matches the signal class, and the recorded simulation verdict.

## What to verify

- Every async input to a destination domain passes through at least a
  two-stage synchronizer (two flops, nothing between them).
- No multi-bit data bus is synchronized bit-by-bit with 2-FF pairs.
- Counters/pointers that cross are gray-encoded; FIFO pointers use gray
  code and a 2-FF synchronizer.
- No false-path or max-delay constraint is used to silence a crossing that
  has no synchronizer.
- Pulses are not fed to level synchronizers.
- The simulation/verification recorded in the evals README reproduces on
  this machine.

## How to verify

```
python examples/good/cdc_two_flop_sync.py        # coherent single-bit
python examples/bad/cdc_multibit_naive_sync.py   # mixed-word capture

# Target flow (toolchain documented; verilator not installed on this host):
verilator --lint-only examples/good/cdc_two_ff_sync.v
verilator --lint-only examples/good/cdc_gray_counter.v
verilator --lint-only examples/bad/cdc_single_ff_sync.v
```

Researched — the `verilator --lint-only` commands are the documented
verification for the Verilog files; on this host only the Python CDC models
were executed (outputs recorded in `evals/README.md`).

## Where the knowledge comes from

- `cummings-cdc` — CDC classification and design techniques (Cliff
  Cummings papers); the classify-before-fix method.
- `xilinx-ug` — Xilinx/AMD user guides: CDC methodology, synchronizer
  requirements, constraints vs correct exceptions.
- `verilator-docs` — `--lint-only` for structural checks.
- `sdc-spec` — what `set_false_path` actually does (constraint semantics),
  referenced by `hdl-constraints-authoring`.

## Related skills

- `hdl-constraints-authoring` — writing SDC; correct exceptions vs hiding
  a CDC bug.
- `hdl-timing-closure` — timing reports that look green for the wrong
  reason.
- `meta-verification-harness-validity` — the Python models are the
  harnesses here; they must fail on the bad pattern (they do).
- `embedded-volatile-and-memory-ordering` — adjacent discipline for
  register access across boundaries in software.

## Evaluation

- Synthetic: `bad/cdc_single_ff_sync.v`, `bad/cdc_multibit_naive_sync.v`,
  `bad/cdc_false_path_is_not_a_sync.v` must be flagged; the classification
  (level vs pulse vs counter vs data) must be stated.
- False-positive: `good/cdc_two_ff_sync.v`, `good/cdc_gray_counter.v`,
  `good/cdc_async_fifo_gray.v` must NOT be flagged; a legitimate
  false_path between genuinely asynchronous-but-unrelated blocks is not a
  CDC bug.
- Adversarial: the naive per-bit 2-FF design looks safe in a unit review
  (each bit has two flops) yet produces mixed words — caught only by the
  runnable model and the classification step.
- Verified facts: the two Python models executed on 2026-08-15 (outputs in
  `evals/README.md`); `verilator --lint-only` documented but not run
  (verilator unavailable) — marked UNVERIFIED on this host.
