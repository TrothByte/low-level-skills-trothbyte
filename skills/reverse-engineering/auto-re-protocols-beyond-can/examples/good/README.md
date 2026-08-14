# Good methodology — deterministic protocol RE on a synthetic UART log

Source scripts: `gen_log.py` (capture fixture) and `run_pipeline.py` (the
pipeline). Both are pure Python 3, no dependencies.

## Run it

```
python examples/good/gen_log.py
python examples/good/run_pipeline.py
python examples/good/gen_log.py --clean examples/good/synthetic_clean.log
python examples/good/run_pipeline.py examples/good/synthetic_clean.log
```

`gen_log.py` writes `synthetic_uart.log`: 60 frames of a synthetic UART
protocol with two injected anomalies (frame 19: checksum corruption; frame 37:
counter skip). `--clean` writes the same corpus without anomalies.

## What the pipeline proves

Step 1 (capture) — read the corpus. No decode happens before the whole corpus
exists.

Step 2 (survey) — structure from statistics, not from one frame:
- byte-0 histogram: `0x55` in 60/60 → sync candidate;
- frame-length histogram `{9: 60}` → fixed-length frames;
- distinct values per position: position 2 is constant `5` → length field,
  position 4 is constant `0x00` → the high byte of the small 16-bit value
  (this zero structure is the endianness evidence);
- `0x55` per position: only position 0 → sentinel is position-stable; a `0x55`
  inside the payload is not a sync marker.

Step 3 (correlate) — semantics from correlation:
- byte 1 consecutive deltas: modal delta 1 → 8-bit rolling counter;
- bytes 3-4 as a 16-bit value: both endiannesses read monotonically (59/59),
  so smoothness alone is ambiguous for a counter; the decisive evidence is
  value structure — byte 4 is `0x00` in 60/60 frames while byte 3 is a smooth
  +4 counter — so the data byte sits in the LOW position and the schema says
  little-endian, derived from evidence, not assumed.

Step 4 (bit/field search) — checksum candidate family (XOR-8, sum-8,
two's-complement, additive-carry, CRC-8) over bytes 1..7 vs byte 8: xor8 wins
59/60, with the single miss being the deliberately corrupted frame.

Step 5 (schema) — the derived layout, printed in a fixed format.

Step 6 (verify-as-gate) — per-frame decode with all four checks; the anomalous
log yields `VERDICT: UNCONFIRMED` (exit code 1) and names the failing frames;
the clean log yields `VERDICT: PASS` (exit code 0). The gate is never talked
out of a low score.

## Why this example is good

Every claim printed is derivable from the bytes, and the verify gate is the
final authority. The same scripts on the same log are what `examples/bad`
mangles: there the decoder guesses the boundaries, assumes endianness, and
prints PASS for corrupted frames.
