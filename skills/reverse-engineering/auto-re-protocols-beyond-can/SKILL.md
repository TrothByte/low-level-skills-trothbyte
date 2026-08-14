---
name: auto-re-protocols-beyond-can
description: Use when reverse-engineering a binary protocol from captured bytes — UART/serial frames, industrial fieldbus or CAN-style traffic — where the wire format is unknown and must be recovered from evidence. Applies the deterministic pipeline: capture, survey, correlate, bit/field search, schema, verify-as-gate, instead of guessing field boundaries.
---

# Automated Protocol Reverse Engineering (beyond CAN)

## When to use

- Recovering the wire format of an unknown byte-stream protocol from a captured
  log or live capture (UART/RS-232/RS-485 serial, industrial fieldbus, or
  CAN-style traffic).
- Generalizing the CAN-to-DBC reverse-engineering method to transports whose
  output is a byte schema instead of a DBC file.
- Detecting protocol structure by evidence: sync/sentinel bytes, rolling
  counters, checksums, payload widths, endianness.
- Deciding whether a decode is trustworthy: running the verify-as-gate step
  that answers PASS or UNCONFIRMED.

## When not to use

- A documented protocol with an available spec or datasheet — read the spec;
  reverse engineering is for undocumented protocols.
- Human-readable/text protocols (ASCII line-based, JSON lines, NMEA) — the
  pipeline is for binary field-level recovery.
- Reading ELF/PE binaries, mangled symbols, or compiler/runtime metadata — use
  `go-rust-re` and the ELF/DWARF skills instead.
- Writing a driver or modem init that uses a known register map — that is
  embedded programming, not protocol RE.

## What the agent often gets wrong

- Decoding from a single sample frame by pattern-matching header bytes from
  memory, instead of deriving structure from a whole captured corpus.
- Assuming endianness ("of course it's little-endian") instead of testing both
  interpretations against the data.
- Declaring "there is no checksum" without running a candidate search.
- Ignoring a rolling counter and explaining away counter gaps as "noise".
- Reporting PASS on a log that contains anomalous frames; explaining away a low
  verification score instead of treating it as the gate refusing certification.
- Treating any `0x55`/`0xAA` byte in the stream as a sync marker, instead of
  detecting the sentinel by position stability.

## How to reason correctly

1. Capture a corpus: many consecutive frames, not one.
2. Survey: frame-length histogram, per-position value histogram (heatmap),
   constant positions → structural skeleton.
3. Correlate: rolling counters (constant increment between consecutive
   frames), 16/32-bit fields tested in both endiannesses (decided by value
   structure — constant-byte position/bounds — not by smoothness, which both
   readings satisfy).
4. Bit/field search: run a candidate family (XOR, sum, two's-complement,
   CRC-8, additive-carry) over the surveyed range; the candidate that explains
   the frames and predicts the anomalies wins.
5. Build the schema: field offsets, widths, endianness, checksum coverage.
6. Verify as a gate: re-parse every frame against the schema. All frames pass →
   PASS. Any failure → UNCONFIRMED, with the failing checks named. Never
   explain away a low score.

## What to verify

- The derived schema decodes 100% of the clean frames.
- The checksum candidate is the one that explains all-but-known-corrupt frames
  and also predicts the injected corruption.
- Counter continuity holds across the corpus; gaps are reported as anomalies.
- Endianness is confirmed by value structure (constant-byte position, bounds),
  never assumed and never claimed from smooth deltas alone.
- The gate verdict on a corrupted log is UNCONFIRMED, not PASS.

## How to verify

On this host (Python 3.11.9, Windows; gcc 16.1 present):

```
python examples/good/gen_log.py                       # writes synthetic_uart.log (2 anomalies)
python examples/good/run_pipeline.py                  # UNCONFIRMED, exit code 1
python examples/good/gen_log.py --clean examples/good/synthetic_clean.log
python examples/good/run_pipeline.py examples/good/synthetic_clean.log   # PASS, exit code 0
python examples/bad/decoder_guess.py                  # same log, wrong decode (contrast)
```

Exit code 1 on the anomalous log is the gate working, not a test failure.

## Where the knowledge comes from

- CSS-Electronics CAN-to-DBC pipeline
  (github.com/css-electronics/can-bus-reverse-engineering-skills) —
  methodological inspiration, license UNKNOWN; ideas only, no copied text.
- MITRE CWE (CWE-20, CWE-119/787) — evidence and verification discipline.
- ISO C11 N1570 §6.2.6.2 — endianness is implementation-defined, so it must be
  determined from evidence, never assumed.
- Perry et al. (CCS '23) and Bhatt et al. (CyberSecEval) — the AI
  overconfidence the verify-as-gate step exists to stop.
- Pipeline claims marked VERIFIED were exercised here on a synthetic UART log
  (Python 3.11.9). Real-world protocol framing details are per-device and are
  marked INFERRED until confirmed on target hardware.

## Related skills

- `go-rust-re` — sibling reverse-engineering skill for binary analysis
- `c-undefined-behavior` — integer/UB discipline when implementing decoders in C
- `embedded-volatile-and-memory-ordering` — UART/peripheral register access on
  the capture side (recommend)

## Evaluation

Synthetic: decode the synthetic UART protocol from `synthetic_uart.log` —
detect STX, the 8-bit rolling counter, the 16-bit little-endian counter field,
and the XOR-8 checksum; report UNCONFIRMED on the anomalous log and PASS on the
clean log. Adversarial: endianness swap, counter jump, checksum-failed frame
must not be explained away. False-positive: a clean log must be PASS; a correct
little-endian read must not be "corrected" to big-endian. See `evals/README.md`
for cases, commands, and verified facts.
