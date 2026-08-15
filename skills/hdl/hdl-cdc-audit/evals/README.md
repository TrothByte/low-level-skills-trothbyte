# Evaluation — hdl-cdc-audit

Skill: `skills/hdl/hdl-cdc-audit`. Stability target: `evaluated`. Partial
source-backing: the Python CDC models ran on 2026-08-15 (Python 3.11.9,
Windows); `verilator --lint-only` is documented as target verification but
verilator is NOT installed on this host — those commands are UNVERIFIED here.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/negative | `bad/cdc_single_ff_sync.v` | single flop rejected as insufficient | structural review |
| medium/negative | `bad/cdc_multibit_naive_sync.v` | per-bit 2-FF on data bus flagged | structural review |
| medium/negative | `bad/cdc_false_path_is_not_a_sync.v` | constraint-without-sync flagged | structural review |
| hard/negative | `bad/cdc_multibit_naive_sync.py` | model captures mixed words | exit 0, words `0000`,`1000` |
| easy/positive | `good/cdc_two_ff_sync.v` | two-stage sync accepted | structural review |
| medium/positive | `good/cdc_gray_counter.v` | gray-encoded counter accepted | structural review |
| hard/positive | `good/cdc_async_fifo_gray.v` | gray-pointer async FIFO accepted | structural review |
| positive | `good/cdc_two_flop_sync.py` | single-bit sync coherent | exit 0, latency 1, no glitch |

Detection rule: for every crossing, name the signal class (level/pulse/
counter/data) and require a mechanism valid for that class. Any multi-bit
data bus with per-bit 2-FF is flagged regardless of flop count.

## False-positive evals (correct code must NOT be flagged)

- `good/cdc_two_ff_sync.v` — legitimate single-bit level sync.
- `good/cdc_gray_counter.v` — counter with one-bit-change gray encoding.
- `good/cdc_async_fifo_gray.v` — correct async FIFO (gray pointers, 2-FF
  sync on pointers, full/empty from gray comparisons).
- A `set_false_path` on a genuinely unrelated, correctly-synchronized
  asynchronous interface is NOT a CDC bug — constraints are legal there
  (see `hdl-constraints-authoring`).

## Historical evals

- No formal CDC CVE registry entry is part of the core eval set; the
  failure class is instead documented from Cliff Cummings' published
  classification (cummings-cdc) and Xilinx/AMD methodology guides
  (xilinx-ug). UNVERIFIED: no on-tool confirmation available (verilator
  absent).

## Adversarial evals

- `bad/cdc_multibit_naive_sync.v` passes a per-bit unit review (each bit
  has two flops, looks textbook-correct) but produces incoherent words.
  The Python model `bad/cdc_multibit_naive_sync.py` is the oracle that
  exposes it: source `0101→1010`, destination captured `0000` then `1000`
  in intermediate cycles — words that never existed on the source bus
  (recorded exit 0, 2026-08-15).
- `bad/cdc_false_path_is_not_a_sync.v` compiles/lints cleanly and timing
  reports would go green — the violation is invisible to the timing flow.
  Only the classify-then-fix discipline (rule 4) catches it.

## Verification commands (ACTUAL, recorded 2026-08-15)

```
python examples/good/cdc_two_flop_sync.py
  dest sequence: [0,0,0,0,1,1,1,1,0,0,0,0,1,1,1,1]
  source-to-dest delay: 1 dest cycles
  spurious glitch: False
  VERIFIED: single-bit 2-FF synchronizer is coherent
  exit 0

python examples/bad/cdc_multibit_naive_sync.py
  source bus: ['0101','0101','0101','1010','1010','1010']
  destination: ['0101','0101','0101','0000','1000','1010','1010']
  INCOHERENT values captured that never existed on the source:
    0000, 1000
  VERIFIED: naive per-bit 2-FF sync of a multi-bit bus is unsafe
  exit 0
```

## Verification commands (RESEARCHED, toolchain not available)

```
verilator --lint-only examples/good/cdc_two_ff_sync.v
verilator --lint-only examples/good/cdc_gray_counter.v
verilator --lint-only examples/good/cdc_async_fifo_gray.v
verilator --lint-only examples/bad/cdc_single_ff_sync.v
verilator --lint-only examples/bad/cdc_multibit_naive_sync.v
verilator --lint-only examples/bad/cdc_false_path_is_not_a_sync.v
```

verilator not installed (Windows MSYS2). The `--lint-only` flag is the
documented verilator lint mode (verilator-docs). All three good files and
the three bad files are structurally complete Verilog-2001; the bad files
carry the `// intentionally incorrect` marker.

## Verified facts

- KNOWN: two-stage sync is the accepted minimum for single-bit levels;
  per-bit 2-FF does not make a data bus coherent; gray code flips one bit
  per step; false_path does not add a synchronizer. Sources: cummings-cdc,
  xilinx-ug, sdc-spec.
- VERIFIED (executed): the two Python CDC models behave as described above.
- UNVERIFIED (toolchain absent): verilator lint results; any hardware
  measurement.

## Scoring

- precision: every flagged crossing must map to a reference rule (1-7) and
  a signal-class mismatch.
- recall: all three bad fixtures detected (single-FF, per-bit multi-bit,
  constraint-as-fix).
- FP-rate: the three good fixtures produce zero flags.
- The classification-first discipline is the recall driver: the naive
  multi-bit case is caught at the classification step before any simulation.
