---
name: reverse-engineering-can-signal-extraction
description: Use when extracting CAN signal layouts from a DBC file or reverse-engineering unknown CAN signals: DBC bit numbering (Intel vs Motorola sawtooth), scale/offset, little/big endian, and the parked-vs-moving gate (SWEEP vs HOLDS) before certifying a signal as speed-like.
---

# Reverse Engineering: CAN Signal Extraction from DBC

## When to use

- Reading a DBC file and decoding frames (cantools or a hand decoder).
- Verifying that a signal's `start_bit`, length, byte order, factor, and offset
  were extracted correctly.
- Reverse-engineering an undocumented signal from captured frames and writing
  its DBC entry.
- Certifying a recovered signal (speed, RPM) only when the capture exercised it.

## When not to use

- Recovering a protocol that is not CAN (UART/serial/factory) — use
  `auto-re-protocols-beyond-can`.
- Understanding CAN physical-layer arbitration/error handling — use
  `iso-11898`.
- Writing an embedded CAN driver — that is embedded programming, not RE.
- Vehicle "intelligence" beyond single-frame decode (e.g. OBD-II PID math).

## What the agent often gets wrong

- Confusing the two DBC bit-numbering conventions: Intel (little-endian, `@1`)
  numbers bits LSB-first within each byte; Motorola (big-endian, `@0`) numbers
  bits in a SAWTOOTH pattern — within each byte the numbers run MSB-first, so
  byte 0 bit 7 is sawtooth 0, not linear 7. A Motorola start bit computed with
  the Intel formula silently moves the signal and produces a wrong value
  (recorded: `7|16@0+` yields 112.64 km/h where `0|16@0+` yields 6.0 km/h).
- Reading the `|` in `16|16@1+` as an endianness marker, or the `@1`/`@0`
  the wrong way around (`@1` IS little-endian/Intel).
- Assuming endianness instead of proving it from value structure.
- Applying scale/offset to the raw value in the wrong order, or forgetting the
  raw value must be sign-extended for `-` (signed) signals.
- Certifying a speed signal from parked data where it HOLDS constant — no
  SWEEP, no evidence. The parked-vs-moving gate (CSS-Electronics method) is
  skipped, and a constant-valued "speed" model is reported as extracted.

## How to reason correctly

1. Parse the DBC line `SG_ Name : start|len@byteorder+ (factor,offset)`:
   `@1` = Intel/LE, `@0` = Motorola/BE; `+` unsigned, `-` signed; start bit,
   length, factor, offset, min/max, unit.
2. Extract by convention: Intel — bit n of the frame is `byte(n/8).bit(n%8)`,
   start bit is the LSB and bits extend upward. Motorola — sawtooth number s
   maps to `byte(s/8).bit(7 - s%8)`, start bit is the MSB and bits extend to
   higher sawtooth numbers.
3. Physical value = raw × factor + offset; for `-` signals sign-extend the raw
   value to `length` bits first.
4. Reverse-engineering: survey the frame layout, then test candidate signals
   in BOTH byte orders against value structure (bounds, continuity, counters),
   never against smoothness alone.
5. Gate the certification by phase: only certify a speed-like signal from
   MOVING data where it SWEEPS; parked data (HOLDS) and cruise data
   (constant at speed) do not certify the signal's formula.

## What to verify

- Decode a fixture frame: the raw value and the physical value (factor×raw+off)
  for at least one Intel and one Motorola signal, plus one signed signal.
- The start_bit of every Motorola signal converted via the sawtooth mapping,
  and cross-checked against the byte it claims to start in.
- The signal certifies with a SWEEP in the moving phase; the parked-phase
  formula is NOT reported as valid.
- A DBC that loads and "decodes" is not proof of correctness — verify against
  ground-truth frames (recorded fixture: 6.0 vs 112.64 km/h for the same frame).

## How to verify

On this host (Python 3.11.9, Windows): `cantools` is NOT installed, so the
bit-numbering and gate logic is verified with the self-contained decoder:

```
python examples/good/decode.py examples/good/vehicle.dbc
python examples/good/park_moving_gate.py examples/good/vehicle.dbc
python examples/bad/parked_formula.py examples/good/vehicle.dbc   # must be rejected
```

Target verification command (when cantools is available):

```
python -c "import cantools; db=cantools.load('vehicle.dbc'); \
print(db.get_message_by_name('VehicleState').decode(bytes.fromhex('0258000002260046')))"
```

## Where the knowledge comes from

- `dbc-spec` — DBC signal line grammar, scale/offset, little/big endian,
  sawtooth bit numbering.
- `cantools-docs` — `load`/`decode_message` semantics; the reference tool for
  DBC parsing on the target machine.
- `iso-11898` — CAN frame structure and bit numbering on the wire.
- CSS-Electronics CAN-to-DBC pipeline (methodological inspiration; license
  UNKNOWN — ideas only, no copied text), including the parked-vs-moving gate.
- Empirical: Python 3.11.9 self-contained decoder verified on this host
  2026-08-15 (recorded decodes in `evals/README.md`); cantools verification
  command documented for the target machine.

## Related skills

- `auto-re-protocols-beyond-can` — generalizing DBC recovery to non-CAN byte
  protocols
- `reverse-engineering-shellcode-analysis` — byte-order discipline applied to
  raw bytes
- `binary-analysis-type-recovery` — reading widths/fields from bytes
- `meta-evidence` — evidence gates for "certified" results

## Evaluation

Synthetic: decode the fixture frames — EngineSpeed must be 1090.125 rpm,
CoolantTemp 24.0, Gear 2 (Intel); WheelSpeedFL 6.0, WheelSpeedFR 5.5,
BrakePedal 35.0 (Motorola); any deviation is a bit-numbering bug.
False-positive: `@1`/`@0` and the sawtooth mapping must not be "corrected"
into the other convention; a correct DBC decodes to the exact fixture values;
parked-only data must NOT be certified as speed.
Historical: CSS-Electronics parked-vs-moving gate — speed recovered from parked
data is the documented failure; `bad/parked_formula.py` reproduces it.
Adversarial: `bad/vehicle_bad.dbc` (start_bit `7` instead of `0`) loads and
decodes but yields 112.64 instead of 6.0 — catch it against ground truth;
a signed signal with the sign-extension omitted must be caught.
Commands and verified facts: `evals/README.md`.
