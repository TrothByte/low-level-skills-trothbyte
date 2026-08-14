# Evaluation — auto-re-protocols-beyond-can

Skill: `skills/reverse-engineering/auto-re-protocols-beyond-can`. Toolchain:
Python 3.11.9, gcc 16.1 (present), Windows. Claims marked VERIFIED below were
exercised in this run against `examples/good/synthetic_uart.log`.

## Synthetic evals

- **easy/positive**: given the synthetic log, detect STX at byte 0 (60/60
  frames), the fixed 9-byte frame length, the length field at byte 2, and the
  rolling 8-bit counter at byte 1 (modal delta 1). VERIFIED.
- **medium/positive**: detect the 16-bit little-endian sample field at bytes
  3-4 using the constant-byte evidence (byte 4 is `0x00` in 60/60 frames) and
  recover its values. VERIFIED.
- **medium/positive**: candidate checksum search selects XOR-8 over bytes 1..7
  at byte 8; the winner explains all-but-known-corrupt frames. VERIFIED.
- **medium/negative**: the injected corruption (frame 19, payload bit flip
  without checksum recompute) and counter skip (frame 37) must be reported as
  anomalies, and the gate verdict on the full log must be UNCONFIRMED
  (exit code 1). VERIFIED.
- **hard/adversarial**: a "smooth counter" field in both endiannesses must NOT
  be called little-endian from monotonicity — the byte-swap of a constant-
  increment counter is itself near-constant (LE 59/59 AND BE 59/59 monotone).
  Endianness must be decided by value structure or marked UNCONFIRMED.
  VERIFIED.
- **hard/adversarial**: a checksum mismatch must not be absorbed ("device
  noise"); the schema must be revised or the verdict kept UNCONFIRMED.
  VERIFIED.

## False-positive evals (correct behavior must not be flagged)

- A clean corpus must yield `VERDICT: PASS` and exit code 0 — the gate must
  not flag a healthy log. VERIFIED on `synthetic_clean.log`.
- The correct little-endian read (`0x0001` from raw bytes `01 00`) must not be
  "corrected" to big-endian `0x0100`. VERIFIED.
- A payload byte equal to the sync value `0x55` must not be treated as a frame
  start; only the position-stable byte 0 is the sync. VERIFIED.
- The checksum candidate search must not declare "no checksum" when XOR-8
  explains the frames. VERIFIED.

## Verification commands (executed on this host)

```
python examples/good/gen_log.py                                              # exit 0
python examples/good/run_pipeline.py                                         # exit 1 (UNCONFIRMED: gate working)
python examples/good/gen_log.py --clean examples/good/synthetic_clean.log    # exit 0
python examples/good/run_pipeline.py examples/good/synthetic_clean.log       # exit 0 (PASS)
python examples/bad/decoder_guess.py                                         # exit 0 (wrong decode, contrast)
python tools/tokens/token_measure.py skills/reverse-engineering/auto-re-protocols-beyond-can
```

Note: `tools/tokens/token_measure.py` counts only `.c/.h/.rs/.md` files, so the
Python examples are excluded from its estimate.

## Verified facts (Python 3.11.9, Windows)

| Fact | Command | Observed |
|---|---|---|
| 60-frame, 9-byte fixed corpus | `run_pipeline.py` | `[survey] corpus: 60 frames, 9 bytes each`; length histogram `{9: 60}` |
| byte 0 is the sync (0x55) | survey | `byte-0 values: [(85, 60)]`; `0x55 occurrences per byte position: [60, 0, 0, 1, 0, 0, 0, 8, 0]` |
| length field at byte 2, high byte at byte 4 | survey | `constant positions: [0, 2, 4]` |
| rolling counter at byte 1 | correlate | `byte-1 consecutive deltas: [(1, 57), (3, 1), (255, 1)]`; modal delta 1 in 57/59 |
| smoothness cannot decide endianness | correlate | `monotone-increasing LE 59/59, BE 59/59` |
| endianness from constant-byte position | correlate | `byte3 zero in 0/60, byte4 zero in 60/60` → little |
| checksum candidate search | bitsearch | `xor8: 59/60` wins; sum8/twos_comp/add_carry near 0; the single miss is frame 19 |
| gate refuses the corrupted log | run_pipeline | `[gate] anomalies at frames: [19, 37, 38]`; `VERDICT: UNCONFIRMED`; exit code 1 |
| gate passes the clean log | run_pipeline on `synthetic_clean.log` | `VERDICT: PASS`; exit code 0 |
| bad decoder contradicts the gate | `decoder_guess.py` | same log reported `decoded 60/60 frames, all PASS`, status `0x0100` vs true `0x0001` |

## Eval assertions (what an agent must output)

- Schema: `stx 0x55@0, counter 8-bit rolling@1, length 8-bit@2, sample 16-bit
  little-endian@3-4, payload@5-7, checksum xor8(bytes1..7)@8`.
- On the anomalous log: `UNCONFIRMED` naming frames 19 and 37-38, never PASS.
- On the clean log: `PASS`.
- On a hypothetical field whose byte-swap is equally smooth and has no
  constant/bounded byte: `endianness UNCONFIRMED`, never an assumption.
