# HDL Constraints Authoring — Reference Rules

Knowledge layer for `hdl-constraints-authoring`. Format: RULE → WHY AI
GETS IT WRONG → CORRECT REASONING → EXAMPLE (bad) → COUNTEREXAMPLE (good)
→ VERIFICATION → SOURCE. Uncertainty marked KNOWN / INFERRED / UNVERIFIED.

The `yosys`/`nextpnr` toolchain is NOT installed on this host; commands are
documented and marked UNVERIFIED as runs. SDC semantics are KNOWN from
sdc-spec. Relative paths assume the skill directory as CWD.

## 1. One create_clock per physical source, on the real clock port

- **RULE**: each physical clock source gets exactly one `create_clock`,
  defined on the actual clock port (or generated-clock source), with the
  real period and waveform. Defining a clock on a data pin, or defining
  two clocks on the same port, makes the analysis undefined — tools pick
  one arbitrarily or error.
- **WHY AI GETS IT WRONG**: agents copy clock definitions from snippets and
  paste a second `create_clock` when they "want a faster clock for tests",
  or define the clock on the first register they see instead of the port.
- **CORRECT REASONING**: the clock is a property of the source. One source,
  one definition. Derived clocks use `create_generated_clock` with the
  master and divider factors, never a second `create_clock` on the same
  net.
- **EXAMPLE** (bad): `examples/bad/bad_clocks.sdc` — `clk` defined at
  10 ns and `clk_bogus` at 7.5 ns on the same port, plus a clock defined on
  `data_in`.
- **COUNTEREXAMPLE** (good): `examples/good/good.sdc` — a single
  `create_clock -name clk -period 10.000` on `[get_ports clk]`.
- **VERIFICATION**: SDC parser/lint rejects or warns on the second
  definition and the data-pin clock. Commands: the documented yosys/nextpnr
  flow (UNVERIFIED here).
- **SOURCE**: sdc-spec (create_clock); xilinx-ug.

## 2. Unconstrained inputs are excluded, not analyzed

- **RULE**: a path with no `set_input_delay`/`set_output_delay` is
  silently excluded from timing analysis. An unconstrained port does NOT
  make the report "more green" in the sense of passing — it makes the
  report say nothing about that path.
- **WHY AI GETS IT WRONG**: agents see a green report and conclude "timing
  is fine", including for ports they never constrained. They treat
  "no warning" as "analyzed and passed", but the tool never analyzed them.
- **CORRECT REASONING**: completeness is checked explicitly. Every port
  must have a delay constraint or a written justification for being
  unconstrained. `check_timing` is the tool that lists unconstrained
  paths; a clean `check_timing` is a precondition for trusting the report.
- **EXAMPLE** (bad): `examples/bad/bad_io_delays.sdc` — `data_in` has no
  `set_input_delay` at all; the input-to-register path is silently
  skipped.
- **COUNTEREXAMPLE** (good): `examples/good/good.sdc` — `set_input_delay
  2.0` for `data_in`, `set_output_delay 3.0` for `data_out`, then
  `check_timing -verbose` as the completeness gate.
- **VERIFICATION**: `check_timing` reports the unconstrained path in the
  bad file and none in the good file. UNVERIFIED as a run here; the SDC
  semantics are KNOWN.
- **SOURCE**: sdc-spec (set_input_delay/set_output_delay, check_timing);
  xilinx-ug.

## 3. Every exception is narrow and justified

- **RULE**: `set_false_path`, `set_max_delay`, and `set_multicycle_path`
  are scoped to specific objects and carry a written functional
  justification. Blanket exceptions over `[all_inputs]`/`[all_outputs]`
  hide real violations and are a constraint-authoring defect, not a
  convenience.
- **WHY AI GETS IT WRONG**: blanket exceptions are the fast path to a
  green report, and the SDC has no syntax forcing a reason, so agents ship
  them.
- **CORRECT REASONING**: an exception is legal only when the path is
  genuinely exempt: a synchronized CDC input (false), a known-static
  signal (false), a real multi-cycle operation (multicycle with setup/hold
  counts). Write the reason as a comment on the same line.
- **EXAMPLE** (bad): `examples/bad/bad_exceptions.sdc` — false path
  between `data_in` and `data_out` (a functional path) plus
  `set_max_delay 3.0 -from [all_inputs] -to [all_outputs]`.
- **COUNTEREXAMPLE** (good): `examples/good/good.sdc` — a scoped false
  path on `async_in` (synchronized in RTL) and a documented
  `set_multicycle_path 4 -setup` on the accumulator.
- **VERIFICATION**: structural audit: every exception names a specific
  object and has a justification comment. UNVERIFIED as a run; the rule is
  KNOWN.
- **SOURCE**: sdc-spec (set_false_path, set_multicycle_path, set_max_delay);
  xilinx-ug; cummings-cdc (which CDC paths are genuinely false).

## 4. SDC object names must match the netlist exactly

- **RULE**: SDC references objects by name (ports, pins, regs, nets). If a
  referenced object does not exist in the synthesized netlist, the
  constraint silently constrains nothing. `check_timing` and lint list
  these; every one is a real defect, not a warning to ignore.
- **WHY AI GETS IT WRONG**: agents copy constraints from an earlier
  version of the RTL or guess names (`enable` vs `enable_n`, dropped
  `_n`, case differences, wrong bus ranges) and never run check_timing.
- **CORRECT REASONING**: reconcile the SDC against the actual netlist
  object list. Naming must be exact including case and suffixes; bus
  ranges must match. Then run the lint gate and make it zero-warning.
- **EXAMPLE** (bad): `examples/bad/constraints_mismatched_objects.v` +
  the SDC comment inside it — RTL has `enable_n`, SDC references
  `enable_n` while the author's mental model uses `enable`; the enable
  path stays unconstrained.
- **COUNTEREXAMPLE** (good): `examples/good/good.sdc` names `data_in`,
  `data_out`, `async_in`, `acc` — all present in
  `examples/good/constraints_authoring_target.v`.
- **VERIFICATION**: `check_timing`/lint zero unknown-object warnings on
  the good pair; the bad pair produces a warning on the missing object.
  UNVERIFIED as a run (no toolchain); the matching rule is KNOWN.
- **SOURCE**: sdc-spec (object references); xilinx-ug (linting).

## 5. Delay values come from the real external budget, not the report

- **RULE**: `set_input_delay`/`set_output_delay` model the external timing
  environment (downstream FF-to-FF path, board trace, external setup). The
  values must come from the interface datasheet/budget. Choosing values
  "until the report is green" makes the constraints a lie and the report
  meaningless.
- **WHY AI GETS IT WRONG**: agents tune delays to close timing instead of
  to model reality, because that is the shortest path to a passing number.
- **CORRECT REASONING**: write the external budget first, then constrain.
  If the report fails with honest values, the design needs the fix
  (pipelining — see `hdl-timing-closure`), not weaker constraints.
- **EXAMPLE** (bad): `examples/bad/bad_io_delays.sdc` — output delay of
  8.0 ns on a 10 ns clock with no basis in the external budget.
- **COUNTEREXAMPLE** (good): `examples/good/good.sdc` — 2.0 ns input /
  3.0 ns output with the comment "external FF-to-FF + board delay".
- **VERIFICATION**: the delay numbers in the SDC match the project's
  interface budget document. UNVERIFIED as a run; the principle is KNOWN.
- **SOURCE**: xilinx-ug; sdc-spec.

## 6. Constraint authoring is part of the timing gate

- **RULE**: the constraint file participates in the timing verdict the same
  way a test harness participates in a test verdict. A constraint set that
  can be made to pass any design (by adjusting values or adding
  exceptions) is a void gate — it cannot fail, so it proves nothing.
  Authoring constraints that honestly model the spec is the precondition
  for any timing closure claim.
- **WHY AI GETS IT WRONG**: agents treat the SDC as an incidental file
  and the timing report as the authority. The report is only as sound as
  the SDC (rule 1-5), and the SDC is written by the same agent that wants
  a green number.
- **CORRECT REASONING**: author the SDC before running the flow, freeze it,
  then let the report judge the design. Any post-hoc SDC edit that changes
  the verdict must be a documented, justified exception.
- **EXAMPLE** (bad): tuning delays or adding blanket exceptions after
  seeing a failing report.
- **COUNTEREXAMPLE** (good): the frozen `good.sdc`; report verdicts come
  from the design, not the constraints.
- **VERIFICATION**: the SDC diff between "before fixing the design" and
  "after" must be empty or contain only justified exceptions.
  Cross-referenced with `meta-verification-harness-validity`.
- **SOURCE**: sdc-spec; xilinx-ug; meta-verification-harness-validity.

## Quick reference table

| SDC practice | Correct | Common wrong |
|---|---|---|
| clock definition | one create_clock per source, on the port | duplicate clocks, data-pin clock |
| I/O constraints | set_input/output_delay from real budget | missing delays, tuned-to-green values |
| exceptions | narrow, justified, object-scoped | blanket false_path / max_delay |
| object names | exact match with netlist | stale/mismatched names |
| completeness | check_timing zero-warning | trusting green report without lint |
