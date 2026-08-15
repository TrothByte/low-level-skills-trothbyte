# hdl — Skills

Low-level engineering skills for this domain.

## Skills in this domain

| Skill | What it does | Stability | Path |
|---|---|---|---|
| `hdl-cdc-audit` | Use when reviewing or fixing clock domain crossing in HDL: classifying the crossing, choosing the right synchronization (2-FF for single-bit, gray-code/handshake/FIFO for multi-bit), and rejecting constraints that hide a CDC bug instead of fixing it. Prevents per-bit 2-FF on multi-bit buses and single-FF "synchronizers". | researched | `skills/hdl/hdl-cdc-audit` |
| `hdl-constraints-authoring` | Use when writing, reading, or triaging SDC timing constraints for FPGA/ASIC flow: create_clock, input/output delays, false/multicycle paths, and lint triage of constraint-vs-netlist mismatches. Prevents duplicate clocks, blanket exceptions, unconstrained paths, and constraint/RTL object mismatches. | researched | `skills/hdl/hdl-constraints-authoring` |
| `hdl-timing-closure` | Use when closing timing or judging a timing report in FPGA/ASIC flow: distinguishing real fixes (pipelining, retiming, shorter logic) from report-hiding (false_path on a real path, loosened clock, max_delay crutch). Prevents certifying a green WNS that hides a real setup violation. | researched | `skills/hdl/hdl-timing-closure` |

## How to use

- Read the `SKILL.md` of a skill for its triggers, reasoning rules, and verification commands.
- `references/` hold the deep knowledge; `examples/good|bad` are verified compile-and-run fixtures;
  `evals/` define how the skill is tested.
- Load only the skill you need (see `skills/_meta/meta-routing`); references load on demand.

## Related

- [Back to repository root](../../README.md)
