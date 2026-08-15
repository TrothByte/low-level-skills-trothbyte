# HDL Clock Domain Crossing — Reference Rules

Knowledge layer for `hdl-cdc-audit`. Format: RULE → WHY AI GETS IT WRONG →
CORRECT REASONING → EXAMPLE (bad) → COUNTEREXAMPLE (good) → VERIFICATION →
SOURCE. Uncertainty marked KNOWN / INFERRED / UNVERIFIED.

The Python CDC models in `examples/` were executed on 2026-08-15
(Python 3.11.9, Windows). `verilator` is NOT installed on this host; its
`--lint-only` usage is documented as target verification and marked
UNVERIFIED. Relative paths assume the skill directory as CWD.

## 1. Classify the signal before choosing the synchronizer (Cummings method)

- **RULE**: the first step of any CDC review is classification of the
  crossing by signal type: single-bit level, single-bit pulse,
  counter/pointer that changes by one per step, or arbitrary multi-bit
  data. The correct mechanism follows from the class: 2-FF for level;
  pulse synchronizer for pulses; gray-code + 2-FF for counters; handshake
  or async FIFO for multi-bit data. Choosing a mechanism without the
  classification is the root cause of most CDC bugs.
- **WHY AI GETS IT WRONG**: models match the phrase "clock domain crossing"
  to the most frequently seen fix ("2-FF synchronizer") and apply it
  uniformly. Multi-bit buses, pulses, and counters each get the same
  treatment, which is wrong for all three.
- **CORRECT REASONING**: state the class out loud, then enumerate the
  mechanisms that are valid for that class. If the class is "multi-bit data
  bus", the 2-FF option is already off the table — not "2-FF plus a
  comment".
- **EXAMPLE** (bad): `examples/bad/cdc_multibit_naive_sync.v` — a 4-bit
  data bus, each bit through its own 2-FF pair. Individually each bit is
  metastability-safe; collectively the bus is incoherent.
- **COUNTEREXAMPLE** (good): `examples/good/cdc_gray_counter.v` — a
  counter (changes by 1) is gray-encoded, so only one bit changes per step;
  `examples/good/cdc_async_fifo_gray.v` — arbitrary multi-bit data goes
  through an async FIFO with gray-coded pointers.
- **VERIFICATION**: `python examples/bad/cdc_multibit_naive_sync.py`
  records mixed words 0b0000 and 0b1000 that never existed at the source
  (recorded exit 0, 2026-08-15). The classification step predicts this
  before any simulation.
- **SOURCE**: cummings-cdc (CDC classification papers); xilinx-ug.

## 2. Two flops are the accepted minimum for a single-bit level crossing

- **RULE**: a single-bit level signal crossing into a destination domain
  must pass through at least two cascaded flip-flops clocked by the
  destination clock, with no combinational logic between them. The first
  stage absorbs the (potentially) metastable sample; the second stage
  provides a resolved output. A single flip-flop is not a synchronizer.
- **WHY AI GETS IT WRONG**: agents think "one flop, value sampled, done" —
  they treat metastability as an abstract footnote and assume a register
  always resolves to a valid 0/1 by the time it is read. In practice the
  first flop can sit in a metastable state; a second stage gives it a full
  destination period to resolve.
- **CORRECT REASONING**: the output of the FIRST flop is still unsafe for
  destination-domain logic (it can be mid-transition). Only the second
  flop's output may feed logic. One flop means the domain logic samples a
  possibly-unsettled value.
- **EXAMPLE** (bad): `examples/bad/cdc_single_ff_sync.v` — `sig_dst <=
  sig_src` with a single flop labeled a "synchronizer".
- **COUNTEREXAMPLE** (good): `examples/good/cdc_two_ff_sync.v` — two
  cascaded flops, nothing between them, output from the second.
- **VERIFICATION**: `python examples/good/cdc_two_flop_sync.py` records a
  clean dest sequence with no spurious glitch and a bounded 1-cycle
  latency (recorded exit 0, 2026-08-15). The two-stage structure is the
  structural requirement for real hardware.
- **SOURCE**: cummings-cdc; xilinx-ug (synchronizer usage).

## 3. Per-bit 2-FF synchronization does NOT make a multi-bit bus coherent

- **RULE**: for a multi-bit data bus, placing a 2-FF synchronizer on every
  bit individually is still wrong. Each bit's first stage samples the
  source at a slightly different destination edge (routing/clock skew), so
  the reconstructed word can be a mix of old and new bits that never
  existed at the source. Only gray-coding (one bit changes per step) or a
  handshake/FIFO with flow control eliminates the incoherent capture.
- **WHY AI GETS IT WRONG**: the agent verifies each bit in isolation
  ("bit 3 is properly synchronized") and never reconstructs the whole
  word. The per-bit view looks correct; the word-level view fails.
- **CORRECT REASONING**: verify at word granularity. Ask "can the
  destination observe a combination of old and new bits?" For a data bus
  that changes more than one bit per source period, the answer is yes, so
  per-bit sync is excluded by construction.
- **EXAMPLE** (bad): `examples/bad/cdc_multibit_naive_sync.v` and its
  Python twin `examples/bad/cdc_multibit_naive_sync.py`.
- **COUNTEREXAMPLE** (good): `examples/good/cdc_gray_counter.v` (counter,
  one bit per step) and `examples/good/cdc_async_fifo_gray.v` (arbitrary
  data with gray pointers + full/empty).
- **VERIFICATION**: `python examples/bad/cdc_multibit_naive_sync.py`
  recorded 2026-08-15: source `0101→1010`, destination captured `0000` and
  `1000` in intermediate cycles — words never on the source bus. This is
  the reproduced proof of incoherence.
- **SOURCE**: cummings-cdc (FIFO and multi-bit CDC); xilinx-ug.

## 4. A constraint does not fix a CDC bug

- **RULE**: `set_false_path` (and friends) removes a path from timing
  analysis. It does not add a synchronizer, does not stop the destination
  flop from sampling a metastable value, and does not make a multi-bit
  capture coherent. Timing closure and CDC correctness are independent
  properties; a constraint can only *document* a crossing that is already
  correctly synchronized.
- **WHY AI GETS IT WRONG**: agents see "timing violation on async path →
  apply false_path → clean report → bug fixed". The report is green, so
  the agent declares the crossing solved. The report and the silicon
  disagree.
- **CORRECT REASONING**: the review order is: (1) classify, (2) check RTL
  has the synchronizer/mechanism, (3) only then add the SDC exception as
  documentation. A false_path found in a design WITHOUT the synchronizer
  is evidence the crossing was never fixed.
- **EXAMPLE** (bad): `examples/bad/cdc_false_path_is_not_a_sync.v` — the
  file comment documents the intended `set_false_path`; the RTL has no
  synchronizer on `valid_src`.
- **COUNTEREXAMPLE** (good): a correctly synchronized crossing where the
  SDC documents the already-correct structure (see
  `hdl-constraints-authoring` for the constraint syntax itself).
- **VERIFICATION**: structural review of the RTL (is there a sync stage?)
  must happen before any timing report is accepted. The Python models
  validate the synchronization logic; no timing tool run can substitute.
  On this host the SDC toolchain (yosys/nextpnr) is not installed —
  UNVERIFIED as a run.
- **SOURCE**: sdc-spec (false_path semantics); xilinx-ug (CDC + timing
  methodology); cummings-cdc.

## 5. Pulses need a pulse synchronizer, not a 2-FF level synchronizer

- **RULE**: a single-cycle pulse in the source domain can be missed by a
  level synchronizer in a destination domain that is faster or slower — the
  destination may sample before the pulse arrives or after it has gone.
  Pulse crossing requires a pulse (toggle/edge-detect) synchronizer or a
  handshake that holds the pulse until acknowledged.
- **WHY AI GETS IT WRONG**: pulse and level look identical in an RTL
  listing ("an async bit"), so the generic 2-FF synchronizer is applied.
- **CORRECT REASONING**: classify first (rule 1): if the source semantics
  is "one event, one cycle", the destination must be able to detect it
  regardless of phase ratio. A level synchronizer only guarantees the
  destination eventually sees a level; it does not guarantee it sees a
  one-cycle event.
- **EXAMPLE** (bad): a one-cycle `req_pulse` in `clk_src` domain fed
  straight into a 2-FF level synchronizer; when `clk_dst` is slower, the
  pulse is invisible.
- **COUNTEREXAMPLE** (good): toggle the source (`pulse ^ 1` on event),
  synchronize the level with 2-FF, detect the edge in the destination, and
  use that edge as the received event — the event cannot be lost even if
  the destination is slower.
- **VERIFICATION**: the toggle structure is the documented pulse-sync
  recipe (cummings-cdc, xilinx-ug). Not simulated on this host —
  UNVERIFIED as a run; the structural rule is KNOWN.
- **SOURCE**: cummings-cdc; xilinx-ug.

## 6. Gray-code counters: only one bit changes per step

- **RULE**: consecutive values of a gray code differ in exactly one bit, so
  a destination that samples an intermediate edge (rule 3) can only capture
  the old or the new value, never a mix. Gray-code encoding is therefore
  the standard mechanism for crossing counters and FIFO pointers.
- **WHY AI GETS IT WRONG**: agents treat gray encoding as "extra code" and
  simplify it away, or send the binary counter directly and rely on luck.
- **CORRECT REASONING**: the property that matters is the Hamming distance
  between consecutive source values. Distance 1 → safe under skew;
  distance > 1 → incoherent captures possible. Enforce the encoding at the
  crossing and keep the binary version on each side.
- **EXAMPLE** (bad): a binary 4-bit counter `bin_count` crossing directly
  through per-bit 2-FF syncs — transitions like 0111→1000 change all four
  bits.
- **COUNTEREXAMPLE** (good): `examples/good/cdc_gray_counter.v` — binary
  counter on the source side, `to_gray` XOR-fold, gray through 2-FF syncs,
  `gray_dst` consumed on the destination side.
- **VERIFICATION**: structural check that the crossing net is gray-coded;
  the async FIFO `examples/good/cdc_async_fifo_gray.v` uses the same
  mechanism for both pointers. No local tool run — UNVERIFIED as a run;
  the encoding property is KNOWN (cummings-cdc).
- **SOURCE**: cummings-cdc (gray-code FIFO pointers); xilinx-ug.

## 7. FIFO/handshake for arbitrary multi-bit data

- **RULE**: arbitrary multi-bit data (no one-bit-change guarantee) crosses
  domains via an async FIFO (gray-coded pointers, full/empty flags) or a
  handshake protocol (request/ack with guaranteed settling). The data is
  stable while the pointer/valid crossing is synchronized, so the
  destination never reads a half-written word.
- **WHY AI GETS IT WRONG**: agents model the FIFO as "a memory with two
  clocks" and forget that the pointer crossing must itself be
  gray-coded and synchronized — the pointer is a counter and follows rule
  6.
- **CORRECT REASONING**: the FIFO solves coherence by decoupling the data
  write from the data read: write when not full, read when not empty, and
  only the (gray, single-bit-change) pointers cross. The read/write
  pointers are the only domain-crossing signals.
- **EXAMPLE** (bad): an async FIFO whose pointers cross as plain binary —
  the full/empty logic can glitch and over/under-read the memory.
- **COUNTEREXAMPLE** (good): `examples/good/cdc_async_fifo_gray.v` — gray
  write/read pointers, 2-FF sync on each pointer, `full`/`empty` derived
  from gray comparisons.
- **VERIFICATION**: the module is structurally complete; the pointer sync
  follows rule 6. No toolchain run on this host — UNVERIFIED as a run;
  the mechanism is KNOWN (cummings-cdc, xilinx-ug).
- **SOURCE**: cummings-cdc (FIFO papers); xilinx-ug.

## Quick reference table

| Signal class | Correct mechanism | Common wrong fix |
|---|---|---|
| single-bit level | 2-FF synchronizer | 1-FF "synchronizer" |
| single-bit pulse | toggle + 2-FF + edge detect | 2-FF level sync (misses pulses) |
| counter (Δ=1) | gray-code + 2-FF per bit | binary counter through 2-FF |
| multi-bit data | handshake / async FIFO (gray pointers) | 2-FF per bit (mixed words) |
| any unsynced path | fix the RTL first | set_false_path (constraint ≠ fix) |
