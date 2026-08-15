# HDL Timing Closure — Reference Rules

Knowledge layer for `hdl-timing-closure`. Format: RULE → WHY AI GETS IT
WRONG → CORRECT REASONING → EXAMPLE (bad) → COUNTEREXAMPLE (good) →
VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED / UNVERIFIED.

The `yosys`/`nextpnr` toolchain is NOT installed on this host; commands are
documented and marked UNVERIFIED as runs. Structural review rules are KNOWN
from xilinx-ug methodology and yosys-docs. Relative paths assume the skill
directory as CWD.

## 1. Green is relative to the declared constraints, not to the silicon

- **RULE**: a timing report is meaningful only relative to the constraints
  it was computed with. "WNS >= 0" means: under the declared clock period,
  input/output delays, and exceptions, no path violates. If the declared
  period is loosened or the exceptions hide paths, the same green report
  says nothing about whether the part works at the real operating
  frequency.
- **WHY AI GETS IT WRONG**: the agent treats the report as an absolute
  property ("the design is in timing") and the constraint file as noise.
  It then "fixes" a violation by editing the constraints, which only
  redefines what "green" means.
- **CORRECT REASONING**: the constraint file is part of the specification.
  Timing closure = (RTL + constraints + tool) such that the report is green
  AND the constraints match the real operating conditions. A change to the
  constraints is a change to the spec, not a fix to the design.
- **EXAMPLE** (bad): `examples/bad/timing_loosened_clock.v` — a 100 MHz
  target (10 ns) with `create_clock -period 20.0` instead; the report goes
  green while the logic still fails at 100 MHz.
- **COUNTEREXAMPLE** (good): `examples/good/timing_pipelined.v` — the
  multiply chain is split into two register stages so the design meets the
  real 100 MHz target; the report is green BECAUSE the logic is shorter.
- **VERIFICATION**: re-run the flow with the ORIGINAL constraint file
  after any fix and confirm WNS. Commands:
  `yosys -p "read_verilog -sv ...; synth_ice40 -top ... -json out.json"`
  then `nextpnr-ice40 ... --freq 100 --report out.rpt` (UNVERIFIED here).
- **SOURCE**: xilinx-ug (timing closure methodology); yosys-docs (report
  generation); sdc-spec (create_clock semantics).

## 2. A false_path must be functionally justified; otherwise it hides a violation

- **RULE**: `set_false_path` removes a path from timing analysis entirely.
  It is correct ONLY for paths that are functionally irrelevant: a
  correctly synchronized CDC crossing, a static/known select, an input that
  is guaranteed stable when sampled. Applying it to a functional path makes
  the report green while the hardware still samples invalid data.
- **WHY AI GETS IT WRONG**: the false_path is the highest-visibility
  "timing fix" in training data, so agents reach for it first. They do not
  distinguish "path does not need timing" from "path cannot meet timing".
- **CORRECT REASONING**: for every false_path, write the functional reason
  in one sentence. If the reason is "so the report goes green", the path is
  a violation being hidden, not a false path.
- **EXAMPLE** (bad): `examples/bad/timing_false_path_abuse.v` — the
  `sel -> d` mux path is functionally meaningful and timing-relevant; the
  SDC comments document `set_false_path -from sel -to d`.
- **COUNTEREXAMPLE** (good): a false_path on a 2-FF-synchronized async
  input (see `hdl-cdc-audit` `good/cdc_two_ff_sync.v`) — the synchronizer
  makes the path functionally irrelevant by design.
- **VERIFICATION**: structural audit of the RTL: does the destination
  actually tolerate asynchronous timing? Only CDC or known-static paths
  qualify. UNVERIFIED as a tool run; the criterion is KNOWN.
- **SOURCE**: xilinx-ug; sdc-spec (set_false_path); cummings-cdc (which
  paths are genuinely async).

## 3. max_delay is not a closure crutch

- **RULE**: `set_max_delay` on a path that fails setup does not shorten
  the logic; it redefines the requirement. The hardware still does not meet
  setup at the clock edge. max_delay is for modeling real external timing
  requirements, not for silencing internal violations.
- **WHY AI GETS IT WRONG**: agents use max_delay as a blanket "make it
  pass" knob across `[all_inputs]` or whole blocks, producing a green
  report with no design change.
- **CORRECT REASONING**: internal critical paths are fixed by reducing
  logic delay (pipelining, retiming, smaller operators, algorithm change).
  max_delay appears in constraints only when it reflects a real external
  interface requirement.
- **EXAMPLE** (bad): `examples/bad/timing_max_delay_crutch.v` — a long
  `x*y` path "fixed" by `set_max_delay 3.0 -from [all_inputs] -to q`.
- **COUNTEREXAMPLE** (good): `examples/good/timing_retimed_baseline.v` —
  a balanced two-stage pipeline with the tool's retiming documented as the
  mechanism.
- **VERIFICATION**: before/after WNS on the SAME constraint file after the
  RTL change. UNVERIFIED as a run; the classification is KNOWN.
- **SOURCE**: xilinx-ug; sdc-spec (set_max_delay).

## 4. Pipelining and retiming are the real fixes

- **RULE**: the honest timing fix shortens the critical path: insert
  register stages (pipelining), let the tool retime across stage
  boundaries, reduce operator fan-in (e.g. balanced adder trees), or
  change the algorithm to need fewer cycles. The report improves because
  the logic is actually faster.
- **WHY AI GETS IT WRONG**: pipelining changes the cycle-by-cycle
  semantics (latency increases), so agents avoid it and prefer "simpler"
  constraint edits that don't touch RTL. The RTL edit is the actual job.
- **CORRECT REASONING**: every pipeline stage adds latency but shortens the
  combinational path. Trade-off: latency vs fmax. When the target frequency
  requires it, add the stages and update the interface contract (valid/
  ready or latency parameter).
- **EXAMPLE** (bad): the un-pipelined `acc <= acc + a*b` chain.
- **COUNTEREXAMPLE** (good): `examples/good/timing_pipelined.v` — product
  in stage 1, accumulation in stage 2, `valid` propagated to mark when the
  output is meaningful.
- **VERIFICATION**: pipeline structure is statically verifiable (each
  stage has exactly one register); the WNS improvement at the ORIGINAL
  frequency is the numeric proof. UNVERIFIED as a tool run.
- **SOURCE**: xilinx-ug (pipelining/retiming guidance); yosys-docs.

## 5. QoR must be compared under identical constraints

- **RULE**: QoR metrics (fmax, LUT/FF count, timing score) are comparable
  only between runs with the SAME constraint file and target. A higher
  fmax obtained by loosening the clock is not QoR improvement; it is the
  same hardware under a weaker spec.
- **WHY AI GETS IT WRONG**: agents report "improved from 80 to 120 MHz"
  without noting the constraint file changed between runs.
- **CORRECT REASONING**: fix the constraint file first, then measure.
  Report fmax at the project target period; report utilization separately
  and honestly.
- **EXAMPLE** (bad): `timing_loosened_clock.v` "improved fmax" by changing
  the period from 10 ns to 20 ns.
- **COUNTEREXAMPLE** (good): `timing_pipelined.v` with the same 10 ns
  constraint; the measured fmax genuinely rises after adding stages.
- **VERIFICATION**: keep one constraints file across all runs; the diff of
  the report shows real change. UNVERIFIED as a tool run.
- **SOURCE**: xilinx-ug; yosys-docs.

## 6. The report-hiding discipline is the hardware twin of a void harness

- **RULE**: a timing gate that goes green because the violation was hidden
  is exactly like a test harness that passes because the assertion never
  runs. Both are gates whose predicate has been redefined away. The
  ablation test for a timing gate: deliberately break the critical path
  (add logic delay) and confirm the report goes red. If it cannot go red,
  the gate is not testing the design.
- **WHY AI GETS IT WRONG**: agents certify green reports without ever
  checking the gate is sensitive to real path delays — the same
  "unconditional pass" failure mode as software harnesses.
- **CORRECT REASONING**: the report must be demonstrably sensitive:
  shorter logic → better WNS, longer logic → worse WNS, and every
  exception justified. A green WNS with exceptions on the top failing
  paths is not closure.
- **EXAMPLE** (bad): false_path placed on the top 3 worst paths until the
  report shows no violations.
- **COUNTEREXAMPLE** (good): pipelining the top path and re-running with
  unchanged constraints; WNS improves from the actual path change.
- **VERIFICATION**: the ablation run (broken critical path → red report)
  is the decisive check. UNVERIFIED as a tool run; the principle is KNOWN.
- **SOURCE**: xilinx-ug; meta-verification-harness-validity (the generic
  harness-validity principle, cross-referenced).

## Quick reference table

| Action | Fix or hiding? | Why |
|---|---|---|
| add pipeline stage | FIX | shortens real combinational path |
| retime across stages | FIX | balances stage delays |
| false_path on real functional path | HIDING | report green, silicon still fails |
| false_path on synced CDC input | FIX (exception) | path functionally irrelevant |
| loosen create_clock period | HIDING | redefines the spec, not the logic |
| max_delay crutch on internal path | HIDING | requirement change, no design change |
| set_multicycle_path documented | EXCEPTION | real multi-cycle semantics |
