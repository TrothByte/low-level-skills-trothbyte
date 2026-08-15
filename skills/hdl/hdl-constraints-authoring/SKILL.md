---
name: hdl-constraints-authoring
description: Use when writing, reading, or triaging SDC timing constraints for FPGA/ASIC flow: create_clock, input/output delays, false/multicycle paths, and lint triage of constraint-vs-netlist mismatches. Prevents duplicate clocks, blanket exceptions, unconstrained paths, and constraint/RTL object mismatches.
---

# HDL Constraints Authoring

## When to use

- Writing or editing an SDC file (`.sdc`) for synthesis/PnR.
- Triaging constraint lint warnings (`check_timing`, tool `-lint`).
- Reviewing whether a design is fully and correctly constrained before
  trusting a timing report.
- Deciding whether an exception (false/multicycle path) is legal.

## When not to use

- Deciding whether a timing number is honest — use `hdl-timing-closure`
  (this skill is about authoring; that one is about judging the report).
- CDC synchronization structure — use `hdl-cdc-audit`; constraints only
  document already-correct syncs.
- Physical floorplanning/placement constraints (PACE, regions) — separate
  concern.

## What the agent often gets wrong

- Defines the same clock twice with different periods, or defines a clock
  on a data pin — the flow silently picks one and the analysis is wrong.
- References objects that do not exist in the netlist (`get_ports
  nonexistent_pin`) — `check_timing`/lint flags it, but the agent ignores
  lint and trusts the green report.
- Assumes unconstrained paths are analyzed; they are NOT. An input with no
  `set_input_delay` is silently excluded from the path analysis, so the
  report is green for a reason — the path was never checked.
- Uses blanket `set_false_path`/`set_max_delay` as "make it pass" knobs
  (see `hdl-timing-closure` rule 3) instead of authoring correct
  exceptions with justification.
- Mismatches RTL and SDC object names (`enable` vs `enable_n`, case
  sensitivity) — one signal stays unconstrained while the author believes
  it is covered.
- Forgets that SDC values must match the real interface: input/output
  delays come from the external datasheet/board, not from "whatever makes
  the report green".
- Omits `check_timing`/lint entirely — constraint verification is part of
  authoring, not an optional extra.

## How to reason correctly

1. **Build the clock model first**: one `create_clock` per physical clock
   source, defined on the actual clock port, with the real period and
   waveform. All delay constraints reference this named clock.
2. **Constrain every I/O**: `set_input_delay` for each input path,
   `set_output_delay` for each output path, values from the real external
   timing budget. If a port is unconstrained, name it and say why.
3. **Author exceptions narrowly**: each `set_false_path`/`set_multicycle_path`
   must be justified (CDC sync, known-static, real multi-cycle semantics)
   and scoped to specific ports/paths, not `[all_inputs]`-style blankets.
4. **Reconcile objects**: every name in the SDC must exist in the
   synthesized netlist; run the flow's `check_timing`/lint and treat every
   warning about unknown objects as a real defect.
5. **Verify the report against the spec**: after authoring, the timing
   report at the DECLARED constraints is the artifact; green must mean
   "constrained correctly and met", not "constraints adjusted until green".

## What to verify

- Exactly one `create_clock` per physical source, on the real clock port,
  period matching the target.
- Every input/output port has a delay constraint or a written justification
  for being unconstrained.
- Every exception references real, existing objects and carries a
  justification comment.
- `check_timing`/lint reports zero unknown-object warnings.
- The SDC and RTL agree on object names (case, `_n` suffixes, bus ranges).
- The timing report at the declared constraints is reproducible.

## How to verify

```
# Target flow (yosys + nextpnr; not installed on this host):
yosys -p "read_verilog -sv examples/good/constraints_authoring_target.v; synth_ice40 -top constraints_authoring_target -json out.json"
nextpnr-ice40 --hx8k --package cb132 --json out.json --pcf empty.pcf --sdc examples/good/good.sdc --freq 100 --report out.rpt
# gate: check_timing (sdc parser) accepts the SDC, no unknown-object
#       warnings, report at --freq 100 meets WNS >= 0.

# Lint-style triage (runnable anywhere, conceptual):
#   - grep the SDC for duplicate create_clock on the same object
#   - grep for get_ports names that have no match in the netlist
#   - grep for blanket set_false_path / set_max_delay across all_inputs
```

Researched — `yosys`/`nextpnr` not available on this Windows host. The
commands above are the documented verification; marked UNVERIFIED as a run.

## Where the knowledge comes from

- `sdc-spec` — SDC format semantics: `create_clock`, `set_input_delay`,
  `set_output_delay`, `set_false_path`, `set_multicycle_path`,
  `set_max_delay`, `check_timing`.
- `xilinx-ug` — constraint authoring methodology, object matching,
  linting and timing report reading.
- `yosys-docs` — SDC ingestion and `nextpnr` constraint handling.
- `hdl-cdc-audit`/`hdl-timing-closure` — when exceptions are legal and
  when a report is hiding a violation.

## Related skills

- `hdl-timing-closure` — judges the reports this skill's constraints
  produce; the FIX/HIDING classification decides exception legality.
- `hdl-cdc-audit` — the synchronization that makes a `set_false_path`
  legal.
- `meta-verification-harness-validity` — the constraint file is part of
  the "harness" that produces the timing verdict; a constraint that makes
  every design pass is a void gate.

## Evaluation

- Synthetic: `bad/bad_clocks.sdc` (duplicate/misplaced clocks, unknown
  object), `bad/bad_exceptions.sdc` (blanket false_path/max_delay),
  `bad/bad_io_delays.sdc` (over-green output delay, missing input delay)
  must be flagged; `good/good.sdc` + `good/constraints_authoring_target.v`
  must be accepted.
- False-positive: a documented `set_multicycle_path` on a real multi-cycle
  block, and a scoped false_path on a synchronized CDC input are NOT bugs.
- Adversarial: `bad/constraints_mismatched_objects.v` — the RTL has
  `enable_n`, the SDC references `enable`; the agent believes the path is
  constrained, but `check_timing` silently excludes it.
- Historical: no CVE-level dataset; anti-patterns are documented in
  sdc-spec/xilinx-ug methodology (UNVERIFIED as named failures).
- Researched commands and status: `evals/README.md`.
