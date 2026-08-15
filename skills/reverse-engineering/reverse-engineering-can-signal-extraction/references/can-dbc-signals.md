# CAN Signal Extraction — Reference Rules

Format: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE → COUNTEREXAMPLE
→ VERIFICATION → SOURCE. Source ids refer to registry/sources.yaml.

## 1. The DBC signal line grammar

- **RULE**: `SG_ Name : start|len@byteorder+ (factor,offset) [min|max] "unit"
  receiver`. `start` is the start bit, `len` the bit length, `byteorder` is
  `1` = little-endian (Intel) or `0` = big-endian (Motorola), `+`/`-` is the
  signedness, and the physical value is `raw * factor + offset`.
- **WHY AI GETS IT WRONG**: reads `@1` as "big-endian" or treats the `|` in
  `16|16@1+` as an endianness separator; forgets `-` requires sign extension.
- **CORRECT REASONING**: `@1` is Intel/little-endian (LSB-first bit
  numbering); `@0` is Motorola/big-endian. Factor/offset convert the raw
  unsigned (or sign-extended) value into the physical unit.
- **EXAMPLE** (bad): decoding `16|16@1+` with Motorola bit numbering.
- **COUNTEREXAMPLE** (good): `16|16@1+ (0.125,0)` = 16-bit Intel signal whose
  raw u16 at bits 16..31 becomes `raw * 0.125 + 0`.
- **VERIFICATION**: decode fixture frames and compare to ground truth
  (recorded in `evals/README.md`).
- **SOURCE**: dbc-spec; cantools-docs.

## 2. Intel (little-endian) bit numbering

- **RULE**: bit n of the frame is physical `byte = n // 8`, `bit = n % 8`.
  The start bit is the position of the signal's LSB; bits extend upward
  (to more significant positions).
- **WHY AI GETS IT WRONG**: assumes every convention numbers bits MSB-first;
  or uses the sawtooth mapping for an Intel signal.
- **CORRECT REASONING**: for `16|16@1+`, start 16 = byte 2 bit 0 (LSB), and
  the 16 bits occupy bytes 2..3 read as a little-endian u16.
- **EXAMPLE** (bad): computing the start byte as `16 // 8 = 2` but reading the
  bits MSB-first within byte 2.
- **COUNTEREXAMPLE** (good): frame bytes `[.., 0x11, 0x22, ..]` at bytes 2,3 →
  raw u16 = 0x2211 = 8721 → 8721 × 0.125 = 1090.125 rpm.
- **VERIFICATION**: `python examples/good/decode.py` prints 1090.125 for the
  EngineSpeed fixture (recorded).
- **SOURCE**: dbc-spec; cantools-docs.

## 3. Motorola (big-endian) bit numbering is the sawtooth

- **RULE**: in a Motorola signal the frame bits are numbered in a sawtooth
  pattern: within each byte the numbers run MSB-first. Sawtooth number s maps
  to physical `byte = s // 8`, `bit = 7 - (s % 8)`. The start bit is the
  position of the signal's MSB; bits extend to higher sawtooth numbers.
- **WHY AI GETS IT WRONG**: computes the Motorola start bit with the Intel
  formula. Byte 0 bit 7 is sawtooth 0, not 7; writing `7|16@0+` instead of
  `0|16@0+` shifts the signal and silently changes the decoded value.
- **CORRECT REASONING**: sawtooth 0..7 = byte 0 bits 7..0; sawtooth 8..15 =
  byte 1 bits 7..0. A 16-bit Motorola signal at start 0 occupies bytes 0..1
  read as a big-endian u16 with MSB at byte 0 bit 7.
- **EXAMPLE** (bad): `SG_ WheelSpeedFL : 7|16@0+` — decodes the fixture frame
  to 112.64 km/h instead of 6.0 km/h (recorded).
- **COUNTEREXAMPLE** (good): `SG_ WheelSpeedFL : 0|16@0+` — frame bytes
  `02 58` → raw 0x0258 = 600 → 6.0 km/h.
- **VERIFICATION**: decode the same frame with both DBCs — 6.0 vs 112.64
  (recorded in `evals/README.md`); the correct DBC must match ground truth.
- **SOURCE**: dbc-spec; cantools-docs; iso-11898 (bit numbering context).

## 4. Scale/offset and signedness

- **RULE**: physical = raw × factor + offset. For a `-` (signed) signal the
  raw value is sign-extended to `len` bits BEFORE scaling. The min/max bracket
  is advisory, not enforced by decoders.
- **WHY AI GETS IT WRONG**: applies offset before factor, forgets sign
  extension, or treats min/max as validation that rejects out-of-range frames.
- **CORRECT REASONING**: a signed 8-bit signal with raw 0xFE = -2 (not 254);
  scaling then yields -2 × factor + offset. Factor and offset are always
  applied in that order to the raw (possibly sign-extended) value.
- **EXAMPLE** (bad): CoolantTemp raw 0x40, `(1,-40)`, computed as
  `40 × 1 + (-40)`... correct; the failure is computing `(0x40-40)×1 = 0`.
- **COUNTEREXAMPLE** (good): raw 64 → `64 × 1 + (-40)` = 24.0 degC.
- **VERIFICATION**: fixture CoolantTemp decodes to 24.0 (recorded).
- **SOURCE**: dbc-spec; cantools-docs.

## 5. The parked-vs-moving gate (SWEEP vs HOLDS)

- **RULE**: a speed-like signal is only certifiable when the capture shows it
  SWEEPING through its range. While parked it HOLDS (constant); while cruising
  it may hold too. A formula fit from parked data is an artifact and must not
  be reported as the signal's behavior.
- **WHY AI GETS IT WRONG**: treats "I decoded frames and got a value" as
  "I extracted the signal", ignoring that parked frames never exercise it.
- **CORRECT REASONING**: split the capture by phase (parked vs moving);
  require distinct-value count and range in the MOVING phase; refuse
  certification from parked data. This is the CSS-Electronics
  parked-vs-moving gate.
- **EXAMPLE** (bad): `examples/bad/parked_formula.py` fits
  "WheelSpeedFL ~ 0.0100 km/h" from 20 parked frames and reports success.
- **COUNTEREXAMPLE** (good): `examples/good/park_moving_gate.py` — parked
  phase: sweep=False, distinct=2; moving phase: sweep=True, distinct=20;
  certifies only from the moving phase.
- **VERIFICATION**: run both scripts (recorded: gate exit 0 PASS; parked
  formula exit 0 but must be REJECTED by review).
- **SOURCE**: cantools-docs; iso-11898; CSS-Electronics pipeline
  (methodological, license UNKNOWN).

## 6. Endianness and field layout from evidence, not assumption

- **RULE**: endianness of an unknown field is decided by value structure —
  which interpretation has stable positions, sane bounds, monotone counters —
  not by "of course it's little-endian".
- **WHY AI GETS IT WRONG**: assumes endianness; both readings of a smooth
  counter look "smooth", so smoothness alone proves nothing.
- **CORRECT REASONING**: test both interpretations; the one whose constant-byte
  positions and bounds hold across the corpus wins. Then cross-check the
  motorola start bit with the sawtooth mapping (rule 3).
- **EXAMPLE** (bad): choosing little-endian because "modern cars use it".
- **COUNTEREXAMPLE** (good): a counter byte that increments in byte 4 under
  one reading and jumps between bytes 3/5 under the other — the stable one is
  the byte order.
- **VERIFICATION**: decode fixture frames under both conventions; the Intel and
  Motorola signals in `vehicle.dbc` decode to their recorded ground-truth
  values.
- **SOURCE**: dbc-spec; cantools-docs.

## Quick reference table

| Topic | Rule in one line |
|---|---|
| Signal line | `SG_ N : start|len@order+ (factor,offset)` — `@1` Intel LE, `@0` Motorola BE |
| Intel bits | bit n = byte(n/8).bit(n%8); start = LSB, extends upward |
| Motorola bits | sawtooth s = byte(s/8).bit(7-s%8); start = MSB, extends to higher s |
| Physical value | raw (sign-extended if `-`) × factor + offset |
| Parked-vs-moving | certify speed only from MOVING frames where the signal SWEEPS |
| Endianness | decided by value structure, never assumed |
