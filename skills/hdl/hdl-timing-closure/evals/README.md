# Evaluation — hdl-timing-closure

Skill: `skills/hdl/hdl-timing-closure`. Stability target: `evaluated`
(structural rules KNOWN; tool runs UNVERIFIED — yosys/nextpnr not installed
on this host). The skill's gate discipline is cross-validated by
`meta-verification-harness-validity` (a green-by-hiding report is a void
gate).

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/timing_false_path_abuse.v` | false_path on functional path flagged as hiding | structural |
| easy/negative | `bad/timing_loosened_clock.v` | clock period relaxed to hide violation | structural |
| medium/negative | `bad/timing_max_delay_crutch.v` | max_delay blanket crutch flagged | structural |
| medium/positive | `good/timing_pipelined.v` | pipeline fix accepted (real path shorter) | structural |
| medium/positive | `good/timing_retimed_baseline.v` | balanced stages + documented retiming | structural |
| easy/positive | `good/timing_reported_clean.v` | short path meets 100 MHz honestly | structural |

Detection rule: classify each "closure change" as FIX (path shortened) vs
HIDING (constraint redefined, path unchanged). HIDING changes are always
flagged; FIX changes must be accompanied by a justification and the
before/after WNS on the SAME constraint file.

## False-positive evals (correct exceptions must NOT be flagged)

- A justified false_path on a correctly synchronized CDC input (see
  `hdl-cdc-audit` good fixtures) is legal.
- A documented `set_multicycle_path` for a genuinely multi-cycle operation
  is legal.
- `good/timing_reported_clean.v` — a design that simply meets timing with
  no exceptions must not be "improved" with pipelining that adds latency
  for no reason.

## Historical evals

- The report-hiding anti-patterns (false_path on real path, loosened clock,
  max_delay crutch) are documented in xilinx-ug timing closure methodology
  as wrong practice. UNVERIFIED as specific named silicon failures on this
  host (no toolchain, no case database).

## Adversarial evals

- `bad/timing_false_path_abuse.v`: the design's unit review is clean
  (valid RTL, module compiles), and the report goes green — the violation
  is only visible by auditing the SDC exceptions and asking "is this path
  functionally irrelevant?" The classification discipline is the only
  detection mechanism.
- `bad/timing_loosened_clock.v`: nothing about the RTL is wrong; the
  hiding is in the constraint file. The agent must compare the declared
  period to the project target (100 MHz → 10 ns), not read the report in
  isolation.
- Green-by-hiding as a void gate: cross-referenced with
  `meta-verification-harness-validity` — an unconditional-pass harness and
  a hidden-path timing report fail for the same reason.

## Verification commands (RESEARCHED, toolchain not available)

```
yosys -p "read_verilog -sv examples/good/timing_pipelined.v; synth_ice40 -top timing_pipelined -json timing.json"
nextpnr-ice40 --hx8k --package cb132 --json timing.json --pcf empty.pcf --freq 100 --report timing.rpt
# gate on the report: WNS >= 0 at --freq 100 with UNCHANGED constraints.
# audit: every exception in the SDC must carry a functional justification.
```

yosys/nextpnr not installed (Windows MSYS2). The commands are the documented
verification flow (yosys-docs, xilinx-ug). All six example files are
complete Verilog-2001 modules; the three bad files carry the
`// intentionally incorrect` marker.

## Verified facts

- KNOWN: green reports are relative to declared constraints; false_path
  requires functional justification; max_delay is not a closure crutch;
  pipelining/retiming are the real fixes; QoR must be compared under
  identical constraints. Sources: xilinx-ug, sdc-spec, yosys-docs.
- UNVERIFIED (toolchain absent): any yosys/nextpnr timing number; the
  before/after WNS of the pipeline fix on real silicon models.

## Scoring

- precision: every flagged change maps to the FIX/HIDING classification
  and a reference rule (1-6).
- recall: all three hiding fixtures detected via exception audit.
- FP-rate: the three good fixtures and the legal exceptions produce zero
  flags.
- Decisive test: "does the report still hold with the original constraints
  and no exception on this path?" — a hiding design fails this; a fixed
  design passes.
