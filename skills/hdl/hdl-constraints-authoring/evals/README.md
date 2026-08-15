# Evaluation — hdl-constraints-authoring

Skill: `skills/hdl/hdl-constraints-authoring`. Stability target:
`evaluated` (SDC semantics KNOWN from sdc-spec; tool runs UNVERIFIED —
yosys/nextpnr not installed on this host).

## Synthetic evals

| Case | Fixture | Expected | Status |
|---|---|---|---|
| easy/negative | `bad/bad_clocks.sdc` | duplicate clocks on same port + data-pin clock flagged | structural |
| medium/negative | `bad/bad_exceptions.sdc` | blanket false_path + max_delay flagged | structural |
| medium/negative | `bad/bad_io_delays.sdc` | over-green output delay, missing input delay flagged | structural |
| hard/negative | `bad/constraints_mismatched_objects.v` | SDC/RTL object mismatch flagged via check_timing | structural |
| medium/positive | `good/good.sdc` + `good/constraints_authoring_target.v` | consistent, justified constraints accepted | structural |

Detection rule: verify the SDC with the completeness gate — one clock per
source, every I/O constrained or justified, every exception narrow and
justified, every object name matching the netlist, `check_timing` clean.

## False-positive evals (correct constraints must NOT be flagged)

- `good/good.sdc` — single clock, real delay values, scoped false path on a
  synchronized CDC input, documented multicycle path, `check_timing`.
- A `create_generated_clock` derived from a master clock is legal (not a
  "duplicate clock").
- A justified `set_false_path` on an async input that is synchronized in
  RTL is correct (cross-checked with `hdl-cdc-audit`).

## Historical evals

- Anti-patterns (duplicate clocks, blanket exceptions, tuned-to-green
  delays, stale object names) are documented in xilinx-ug constraint
  methodology and sdc-spec. UNVERIFIED as named silicon failures on this
  host (no case database, no toolchain).

## Adversarial evals

- `bad/bad_io_delays.sdc`: the design looks fully constrained (there IS a
  clock and an output delay); the defects are the *absence* of the input
  delay and the *unrealistic* value of the output delay. An agent reading
  the file quickly sees "constraints exist" and signs off — the
  completeness gate is the only detector.
- `bad/constraints_mismatched_objects.v`: the SDC is never shown — only
  the RTL. The agent must demand the SDC and cross-check object names.
  The mismatch (enable vs enable_n) silently leaves a path unconstrained
  while the author believes it is covered.
- Blanket-exception SDC: report goes green and the agent trusts it — the
  same void-gate failure as `meta-verification-harness-validity`.

## Verification commands (RESEARCHED, toolchain not available)

```
yosys -p "read_verilog -sv examples/good/constraints_authoring_target.v; synth_ice40 -top constraints_authoring_target -json out.json"
nextpnr-ice40 --hx8k --package cb132 --json out.json --pcf empty.pcf --sdc examples/good/good.sdc --freq 100 --report out.rpt
```

yosys/nextpnr not installed (Windows MSYS2). The commands are the documented
verification flow (yosys-docs). The bad SDC/RTL files carry the
`# intentionally incorrect` (sdc) / `// intentionally incorrect` (v)
markers.

## Verified facts

- KNOWN: create_clock on-source rule; unconstrained paths are excluded
  from analysis; exceptions require justification; object names must match
  the netlist; delay values must model the real external budget. Sources:
  sdc-spec, xilinx-ug.
- UNVERIFIED (toolchain absent): yosys/nextpnr acceptance of the SDC,
  check_timing output on the good/bad pairs.

## Scoring

- precision: every flagged constraint maps to a reference rule (1-6).
- recall: all four bad fixtures detected via the completeness gate.
- FP-rate: the good pair and legal exceptions produce zero flags.
- Decisive test: "if I delete this constraint, does check_timing change?"
  — a constraint that never matched an object fails this.
