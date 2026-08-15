---
name: hdl-timing-closure
description: Use when closing timing or judging a timing report in FPGA/ASIC flow: distinguishing real fixes (pipelining, retiming, shorter logic) from report-hiding (false_path on a real path, loosened clock, max_delay crutch). Prevents certifying a green WNS that hides a real setup violation.
---

# HDL Timing Closure and QoR

## When to use

- A timing report shows negative slack (WNS/TNS < 0) and you must decide
  how to fix it.
- Reviewing an agent's "timing closure" change: did it shorten the logic
  or hide the violation?
- Judging whether a design is ready to sign off at a target frequency.
- Estimating or improving QoR (area, LUT count, fmax) in a synthesis flow.

## When not to use

- A pure functional bug with no timing involvement — use
  `hdl-cdc-audit` for CDC, other skills for logic errors.
- Writing constraints from scratch (create_clock, input/output delays) —
  that is `hdl-constraints-authoring`.
- CDC synchronization structure — constraints document it, but the
  synchronizer is an RTL concern (`hdl-cdc-audit`).
- Microarchitecture/floorplaning of a huge SoC — outside this skill's scope.

## What the agent often gets wrong

- "Makes the number green by hiding the violation": adds a false_path on a
  functionally meaningful path, loosens the create_clock period, or drops
  a max_delay. The report passes; the silicon does not.
- Treats a green report as proof the design meets timing at the REAL
  operating frequency. A report is only meaningful relative to the declared
  constraints — change the constraints and the same report proves nothing.
- Applies `set_max_delay` across a broad set of paths to silence violations
  instead of fixing the logic depth.
- Confuses WNS (worst negative slack) with "the design is fine" — WNS can
  be positive while the design is only "green" because a critical path was
  excluded.
- Fixes timing by re-synthesis with a relaxed target instead of addressing
  the critical path (retiming, pipelining, algorithm change).
- Forgets that a false_path must be justified: a path is false only if it
  is functionally irrelevant (e.g. a correctly synchronized CDC crossing
  or a known-static select). Justification is part of the fix, not an
  afterthought.

## How to reason correctly

1. **Read the report before touching constraints**: identify the top
   failing paths and the actual logic delay. The fix is about the path,
   not the number.
2. **Ask "what does green mean here?"** Green means: at the DECLARED
   constraints, no path violates. If the declared constraints are wrong
   (loosened clock), green is meaningless.
3. **Classify each fix**:
   - shorten the path (pipelining, retiming, smaller fan-in, faster carry)
   - reduce the period required (algorithmic change)
   - correct exception (genuinely false or multi-cycle path) — must be
     justified in a comment
   - report-hiding (false_path on a real path, max_delay crutch, loosened
     clock) — always wrong as a closure method.
4. **Verify by re-running with the original constraints** after any
   pipeline change; WNS must improve from the actual path shortening.
5. **Review QoR honestly**: report fmax and utilization with the same
   constraint set across versions; a higher fmax from a loosened clock is
   not QoR improvement.

## What to verify

- WNS/TNS are computed with the target constraints, not a relaxed clock.
- Every false_path/max_delay exception has a written justification (CDC
  synchronizer, static select, multi-cycle path) and is not on a
  functional path.
- Pipelining/retiming changes actually shorten the critical path in the
  report (compare before/after WNS on the SAME constraint file).
- The design meets timing at the target operating frequency declared by
  the project, not at a test-only frequency.
- The evals README records what was run on this machine.

## How to verify

```
# Target flow (yosys + nextpnr; toolchain not installed on this host):
yosys -p "read_verilog -sv examples/good/timing_pipelined.v; synth_ice40 -top timing_pipelined -json timing.json"
nextpnr-ice40 --hx8k --package cb132 --json timing.json --pcf empty.pcf --freq 100 --report timing.rpt
# gate: grep the .rpt for WNS/TNS >= 0 at --freq 100; then audit every
#       exception in the .sdc for a written justification.

# Structural review (runnable on any host):
#   - confirm two/three register stages exist between a*b and acc
#   - confirm no false_path/max_delay crutch lines in the SDC
```

Researched — `yosys`/`nextpnr` not available on this Windows host. The
commands above are the documented verification; marked UNVERIFIED as a run.

## Where the knowledge comes from

- `xilinx-ug` — timing closure methodology, WNS/TNS, false paths and
  correct exceptions, pipelining/retiming guidance.
- `yosys-docs` — `synth_ice40`, JSON netlist, `nextpnr` integration, timing
  report generation.
- `sdc-spec` — the semantics of `create_clock`, `set_false_path`,
  `set_max_delay` (shared with `hdl-constraints-authoring`).
- `cummings-cdc` — which paths are genuinely async/false (CDC), cross
  referenced from `hdl-cdc-audit`.

## Related skills

- `hdl-constraints-authoring` — SDC syntax and lint triage; this skill
  decides whether a constraint is a fix or a hiding.
- `hdl-cdc-audit` — the async paths that legitimately become exceptions.
- `meta-verification-harness-validity` — "the report is green" is a gate
  result; gates must be validated by ablation (a broken design must not
  go green). Closely parallel: report-hiding is the hardware twin of the
  unconditional-pass harness.

## Evaluation

- Synthetic: `bad/timing_false_path_abuse.v`, `bad/timing_loosened_clock.v`,
  `bad/timing_max_delay_crutch.v` must be flagged as report-hiding;
  `good/timing_pipelined.v`, `good/timing_retimed_baseline.v`,
  `good/timing_reported_clean.v` must NOT be flagged.
- False-positive: a justified false_path on a genuinely async CDC input
  (correctly synchronized) is NOT report-hiding; a multi-cycle path with a
  documented `set_multicycle_path` is legal.
- Adversarial: a design whose report is green ONLY because a false_path
  hides the real critical path — unit review passes, silicon fails.
- Historical: timing-hiding anti-patterns documented in xilinx-ug
  methodology (UNVERIFIED as cases without tool runs).
- Researched commands and status: `evals/README.md`.
