# Evaluation — reverse-engineering-can-signal-extraction

Skill: `skills/reverse-engineering/reverse-engineering-can-signal-extraction`.
Stability target: `evaluated`. **RESEARCHED skill**: `cantools` and a real CAN
bus are NOT available on this host. The bit-numbering and gate logic is
VERIFIED with the self-contained Python 3.11.9 decoder in `examples/good/`;
the cantools-based verification command is documented for the target machine.

## Synthetic evals

| Case | Fixture | Expected | Recorded |
|---|---|---|---|
| easy/positive | `good/vehicle.dbc` EngineData frame | Intel decodes correct | `EngineSpeed: 1090.125`, `CoolantTemp: 24.0`, `Gear: 2.0` |
| easy/positive | `good/vehicle.dbc` VehicleState frame | Motorola decodes correct | `WheelSpeedFL: 6.0`, `WheelSpeedFR: 5.5`, `BrakePedal: 35.0` |
| medium/negative | `bad/vehicle_bad.dbc` | Motorola start bit wrong → silent wrong value | loads + decodes; `WheelSpeedFL: 112.64` instead of 6.0 |
| medium/positive | `good/park_moving_gate.py` | parked HOLDS, moving SWEEPS → PASS | parked: sweep=False distinct=2 range 0.00..0.01; moving: sweep=True distinct=20 → `GATE: PASS` exit 0 |
| medium/negative | `bad/parked_formula.py` | parked-only "speed" model must be rejected | exit 0 but reports a constant 0.0100 km/h model — reject by review |

## False-positive evals (correct results must not be flagged)

- The correct Intel signals (`16|16@1+`) must NOT be "corrected" to Motorola,
  and vice versa; `@1` IS little-endian.
- `WheelSpeedFL : 0|16@0+` is correct — its start bit is sawtooth 0 (byte 0
  bit 7), NOT 7; flagging it as "should be 7" is the common FP.
- A speed signal that holds constant while cruising (not just parked) must be
  reported as UNCONFIRMED for certification, not as "speed extracted".
- Out-of-min/max physical values are not an error if the raw decode is right;
  min/max is advisory metadata.

## Historical evals

- CSS-Electronics CAN-to-DBC reverse-engineering pipeline: the
  parked-vs-moving gate (SWEEP vs HOLDS) is its documented methodology —
  recover signal formulas only from phases that exercise them. `bad/parked_formula.py`
  reproduces the failure this gate exists to stop.
- Calibration: like all DBC work, "the file loads and decodes" is not
  correctness — the recorded 6.0-vs-112.64 divergence on the same frame is the
  reference case for why ground-truth frames are mandatory.

## Adversarial evals

- `bad/vehicle_bad.dbc` uses a plausible wrong start bit (`7` instead of `0`)
  — the agent must catch it against the fixture frame, not by reading the DBC.
- A signed signal decoded without sign extension must be caught (raw 0xFE =
  -2, not 254, for `-` signals).
- Swap of factor/offset order (offset×raw+factor) must be caught.
- The parked-vs-moving FP: an agent that certifies a speed model from the
  parked fixture must fail the SWEEP check.

## Verification commands (ACTUAL, recorded 2026-08-15)

```
python examples/good/decode.py examples/good/vehicle.dbc
  EngineData: {'EngineSpeed': 1090.125, 'CoolantTemp': 24.0, 'Gear': 2.0}
  VehicleState: {'WheelSpeedFL': 6.0, 'WheelSpeedFR': 5.5, 'BrakePedal': 35.0}

python examples/good/decode.py examples/bad/vehicle_bad.dbc
  VehicleState: {'WheelSpeedFL': 112.64, 'WheelSpeedFR': 5.5, 'BrakePedal': 35.0}
  # wrong Motorola start bit: same frame, silent wrong value

python examples/good/park_moving_gate.py examples/good/vehicle.dbc
  parked phase : sweep=False distinct=2 range=0.00..0.01 km/h
  moving phase : sweep=True  distinct=20 range=0.00..3.23 km/h
  GATE: PASS — speed signal certified from the MOVING phase only
  (exit 0)

python examples/bad/parked_formula.py examples/good/vehicle.dbc
  speed model fitted from parked data: WheelSpeedFL ~ 0.0100 km/h
  RESULT: speed signal extracted and certified
  (exit 0 — must be REJECTED by review: no sweep exercised)
```

## Target verification (required for full `evaluated` status)

Run the same checks with the reference tool:

```
python -c "import cantools; db=cantools.load('vehicle.dbc'); \
print(db.get_message_by_name('EngineData').decode(bytes.fromhex('0000401122000000'))); \
print(db.get_message_by_name('VehicleState').decode(bytes.fromhex('0258000002260046')))"
# expected: EngineSpeed=1090.125 rpm, CoolantTemp=24.0, Gear=2;
#           WheelSpeedFL=6.0, WheelSpeedFR=5.5, BrakePedal=35.0

valgrind/hardware: capture CAN frames from a real or virtual bus
(python-can + CANable/virtual vcan0 on Linux) and replay the parked/moving
gate against a real drive cycle.
```

## Verified facts (on this host)

- DBC parse + Intel extraction + Motorola sawtooth extraction + scale/offset:
  VERIFIED against hand-computed ground truth (all six fixture values above).
- Wrong Motorola start bit produces a silent wrong value: VERIFIED (112.64 vs
  6.0).
- Parked-vs-moving SWEEP check discriminates the phases: VERIFIED.
- cantools behavior itself: UNVERIFIED on this host (not installed) — the
  documented command must be run on the target machine.

## Scoring (for routing eval)

- precision: every flagged issue maps to a reference rule (1-6) and is
  confirmed by fixture decodes.
- recall: wrong convention, wrong start bit, missing sign extension, wrong
  factor/offset order, and parked-data certification are all detectable.
- FP-rate: correct DBC entries and correct decodes produce zero flags.

## Toolchain status (honest)

- `cantools`: NOT installed; `pip install cantools` not attempted (network
  policy). Verification command provided for the target machine.
- Real CAN hardware / vcan0: NOT available on this Windows host — ISO 11898
  wire-level claims are sourced from `iso-11898` and marked accordingly.
