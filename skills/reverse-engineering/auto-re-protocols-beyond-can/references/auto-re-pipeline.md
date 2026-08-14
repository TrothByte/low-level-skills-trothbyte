# Automated Protocol Reverse Engineering — the deterministic pipeline

Each entry: RULE → WHY AI GETS IT WRONG → CORRECT REASONING → EXAMPLE →
COUNTEREXAMPLE → VERIFICATION → SOURCE.

Claim status: claims marked VERIFIED were exercised on this host (Python
3.11.9, Windows) against the synthetic UART log in `examples/good/`. Claims
about real-world protocol framing are hardware/device-specific and marked
INFERRED until confirmed on target hardware.

Methodological inspiration note: the pipeline generalizes the CAN-to-DBC
reverse-engineering approach from the CSS-Electronics repository
(github.com/css-electronics/can-bus-reverse-engineering-skills). Its license is
UNKNOWN: this skill derives methodological ideas only and copies no text.

## 1. The deterministic pipeline is transport-agnostic: capture → survey → correlate → bit/field search → schema → verify-as-gate

- **RULE**: For CAN the canonical workflow recovers signals and a DBC file from
  raw frames. The same six steps apply to any byte-stream protocol: CAPTURE a
  corpus, SURVEY structure, CORRELATE fields, BIT/FIELD SEARCH the checksum,
  BUILD a byte-field schema (the DBC analogue), and VERIFY-AS-GATE. Transport
  (CAN frame, UART byte stream, fieldbus PDU) only changes the capture layer.
- **WHY AI GETS IT WRONG**: agents "decode" a protocol from one sample frame by
  matching remembered header patterns, treating RE as a naming guess instead of
  a statistical recovery over a corpus.
- **CORRECT REASONING**: structure first, semantics second. Every field in the
  final schema is a hypothesis that must be re-verified against every frame.
- **EXAMPLE** (bad): looking at one frame `55 00 05 01 00 30 40 50 24` and
  announcing "byte 3-4 is a big-endian status word".
- **COUNTEREXAMPLE** (good): over 60 captured frames, byte 0 is `0x55` in
  60/60, byte 2 is `5` in 60/60, byte 1 increments by 1 per frame, and bytes
  3-4 form a 16-bit little-endian field whose high byte is constant `0x00`
  while its low byte is a smooth counter → schema.
- **VERIFICATION**: `python examples/good/run_pipeline.py` on the synthetic
  log prints the derived schema and the gate verdict. VERIFIED (Python 3.11.9).
- **SOURCE**: CSS-Electronics CAN-to-DBC pipeline, inspiration, license UNKNOWN
  (ideas only); `cwe` (CWE-20, CWE-119/787 reasoning discipline);
  `perry-ai-code` (why skipping verification is the failure mode).

## 2. Survey before parse: derive structure from histograms, never from a single frame

- **RULE**: The structural skeleton comes from corpus statistics: frame-length
  histogram (fixed vs variable length), per-position value histograms (which
  positions are constant, which vary), and the set of constant positions (sync,
  length, reserved fields). Only after this do you assign meaning.
- **WHY AI GETS IT WRONG**: agents parse the first frame they see and hard-code
  offsets, so any frame-length variation or optional field misaligns every
  later decode.
- **CORRECT REASONING**: run the survey before touching semantics. A fixed
  frame length with a constant position-0 byte is evidence of a header; a
  variable length with a length field is evidence of framing.
- **EXAMPLE** (bad): "All frames are 9 bytes, so the protocol is fixed-length
  and the header is byte 0." — stated without a length histogram.
- **COUNTEREXAMPLE** (good): `[survey] frame-length histogram: {9: 60}` and
  `[survey] constant positions (sync/length candidates): [0, 2, 4]` — the
  skeleton (byte 4 constant because the 16-bit LE value is small), derived from
  all frames.
- **VERIFICATION**: the `[survey]` block of `run_pipeline.py`. VERIFIED.
- **SOURCE**: CSS-Electronics CAN-to-DBC pipeline, inspiration, license UNKNOWN
  (ideas only); `binutils-docs` (hexdump/xxd/strings for capture review);
  `cwe` (CWE-20).

## 3. Sentinel/sync bytes are detected by position stability, not by value

- **RULE**: A sync byte is a byte that appears at a fixed position in every
  frame. The same value occurring mid-stream is payload. Greedily splitting the
  stream on every `0x55` misaligns frames whenever the payload contains `0x55`.
- **WHY AI GETS IT WRONG**: agents grep the dump for a "magic" byte and slice
  there, ignoring that magic values are not reserved in the payload space.
- **CORRECT REASONING**: count `0x55` per byte position. Position 0 constant in
  100% of frames → sync; sporadic occurrences elsewhere → payload. This is
  sentinel masking: the position masks the sentinel's role.
- **EXAMPLE** (bad): "The stream contains `0x55` here and here — two frames
  starting at those offsets."
- **COUNTEREXAMPLE** (good): `[survey] 0x55 occurrences per byte position:
  [60, 0, 0, 1, 0, 0, 0, 8, 0]` — position 0 is the only sync; the `0x55`
  values at bytes 3 and 7 of individual frames are payload/sample bytes, not
  sync markers.
- **VERIFICATION**: the `[survey]` 0x55-position block of `run_pipeline.py`.
  VERIFIED.
- **SOURCE**: CSS-Electronics CAN-to-DBC pipeline, inspiration, license UNKNOWN
  (ideas only); `cwe` (CWE-20 parsing discipline).

## 4. Rolling counters: detect by constant increment, then use continuity as a gate

- **RULE**: A rolling counter is a field whose value increases by a constant
  delta between consecutive frames and wraps modulo its width. Detection: the
  modal consecutive delta is 1 over nearly all transitions. After detection it
  becomes a gate: any transition whose delta differs from the modal delta
  marks a dropped or replayed frame — an anomaly, not noise.
- **WHY AI GETS IT WRONG**: agents ignore the field as "uninteresting
  sequence", or absorb counter gaps into the decode and silently misorder
  frames.
- **CORRECT REASONING**: treat the counter both as structure evidence and as a
  corruption detector. A duplicate or skipped counter means the capture lost or
  duplicated data; the decode must say so.
- **EXAMPLE** (bad): "The counter field skips sometimes; that's normal noise,
  decode continues."
- **COUNTEREXAMPLE** (good): `[gate:full] counter: 58/60` with
  `[gate] anomalies at frames: 19, 37, 38` — the counter gate flags the
  duplicate/skip at frame 37 and the verdict stays UNCONFIRMED.
- **VERIFICATION**: `[correlate] byte-1 consecutive deltas` and the
  `[gate]` counter line of `run_pipeline.py`. VERIFIED (the synthetic log
  injects a counter skip at frame 37; the gate reports it).
- **SOURCE**: CSS-Electronics CAN-to-DBC pipeline (counter fields in CAN
  signal RE), inspiration, license UNKNOWN (ideas only); `cwe` (CWE-20).

## 5. Checksum/CRC detection is a candidate search, never a guess or a "no checksum" verdict

- **RULE**: With a surveyed field range and a candidate byte position, run a
  small algebra family (XOR-8, sum-8, two's-complement, CRC-8, additive-carry)
  over the range and compare against the candidate byte. The winner explains
  all-but-known-corrupt frames (and predicts the corruption). Reporting "no
  checksum" without running the search is a guess.
- **WHY AI GETS IT WRONG**: agents look at the last byte, see it "varies", and
  conclude it is noise or random padding; or they announce a checksum by
  matching a remembered CRC table entry by eye.
- **CORRECT REASONING**: enumerate the candidate family programmatically; a
  correct candidate reaches ~100% match on clean frames and near-zero on a
  wrong candidate. The mismatch pattern on a corrupted frame identifies which
  frame the wire corrupted.
- **EXAMPLE** (bad): "The trailing byte changes every frame, so the protocol
  has no integrity field."
- **COUNTEREXAMPLE** (good): `[bitsearch] xor8: 59/60 frames match`, with all
  other candidates near 0/60 and the single miss being the deliberately
  corrupted frame → XOR-8 over bytes 1..7 at byte 8.
- **VERIFICATION**: the `[bitsearch]` block of `run_pipeline.py`. VERIFIED.
- **SOURCE**: `iso-c11-n1570` (unsigned arithmetic for the search is
  well-defined; signed overflow in checksum math would be UB, §6.5);
  `cwe` (CWE-20 integrity); CSS-Electronics pipeline, inspiration, license
  UNKNOWN (ideas only).

## 6. Endianness is evidence-derived, never assumed — and smooth deltas alone cannot decide it

- **RULE**: The byte-swap of a constant-increment counter is itself a
  near-constant-increment sequence, so monotonicity/delta-consistency tests
  pass for BOTH endiannesses and do NOT decide. Decisive evidence is value
  structure: a bounded value keeps one byte constant (e.g. `0x00`), so the
  constant byte is the HIGH byte and the varying byte is the LOW byte — which
  fixes the endianness. When no such evidence exists, the schema must say
  `UNCONFIRMED`. The C standard leaves endianness implementation-defined, so
  any assumption is a guess.
- **WHY AI GETS IT WRONG**: agents hard-code "little-endian" from their host
  platform, or believe a smooth delta sequence proves their reading. VERIFIED:
  on the synthetic log both readings are monotone (LE 59/59 and BE 59/59), so
  smoothness alone would be ambiguous.
- **CORRECT REASONING**: build both interpretations; look for constant-byte
  position, value bounds, or cross-field correlation. Record width +
  endianness in the schema explicitly, and mark it UNCONFIRMED when the
  evidence is absent.
- **EXAMPLE** (bad): "Bytes 3-4 read smoothly as a counter, so
  little-endian is confirmed." Smoothness proves neither direction.
- **COUNTEREXAMPLE** (good): byte 4 is `0x00` in 60/60 frames while byte 3 is
  a smooth +4 counter — the data byte sits in the LOW position, the constant
  byte in the HIGH position → little-endian. Schema says
  `16-bit little-endian @ bytes 3-4`.
- **VERIFICATION**: the `[correlate]` endianness lines of `run_pipeline.py`.
  VERIFIED.
- **SOURCE**: `iso-c11-n1570` §6.2.6.2 (object representation; endianness
  implementation-defined); `cwe` (CWE-119 context, misread widths/endianness).

## 7. Verify is a gate, not a report formality: PASS or UNCONFIRMED, never "probably fine"

- **RULE**: The final step re-parses every frame against the schema and runs
  every check (sync, length, checksum, counter continuity). PASS is issued only
  when every frame passes every check. Any failure yields UNCONFIRMED with the
  failing checks named. Explaining away a low score is the single most common
  failure mode and is forbidden.
- **WHY AI GETS IT WRONG**: AI code assistants produce confident-but-wrong
  decodes and rationalize mismatches ("the remaining frames must be from a
  different device"). Overconfidence outpaces accuracy.
- **CORRECT REASONING**: the score is a verdict, not a suggestion. A low score
  means the schema is unconfirmed — go back to the evidence, do not soften the
  conclusion.
- **EXAMPLE** (bad): "59/60 frames matched, the last one is probably a glitch —
  protocol confirmed."
- **COUNTEREXAMPLE** (good): `[gate] VERDICT: UNCONFIRMED -- gate refused to
  certify a clean decode; investigate the anomalies`, followed by re-verifying
  the anomaly-free corpus: `[gate] VERDICT: PASS` (exit code 0).
- **VERIFICATION**: exit codes of `run_pipeline.py`: 1 on the anomalous log,
  0 on the clean log. VERIFIED.
- **SOURCE**: `perry-ai-code` (assistant users write less secure code but
  believe it secure); `cyberseceval` (models emit unsafe, confident output);
  `cwe` (CWE-20, CWE-252).

## 8. The pipeline replaces the DBC: apply the same gates to UART, industrial fieldbus, and radio byte streams

- **RULE**: For CAN the schema is a DBC file; for any other transport it is a
  byte-field schema with the same anatomy: sync/header, counter or sequence,
  length, payload fields (width + endianness), checksum/CRC. The six-step
  pipeline and the verify gate transfer unchanged; only the capture source
  changes (serial dump, logic analyzer, packet capture).
- **WHY AI GETS IT WRONG**: agents treat CAN reverse engineering as a special
  skill and guess at serial protocols, or import CAN-only tools where a byte
  schema is the right abstraction.
- **CORRECT REASONING**: reuse the same evidence loop; export the result as a
  machine-readable schema (JSON/DBC/C) rather than a prose description, so the
  gate can be re-run mechanically.
- **EXAMPLE** (bad): writing a one-off parser that "just handles this UART
  device" with hard-coded offsets and no re-verification.
- **COUNTEREXAMPLE** (good): the schema from `run_pipeline.py` is printed in a
  fixed format, and re-running the gate on a new capture re-validates it.
- **VERIFICATION**: `run_pipeline.py` on the clean synthetic log exits 0 and
  prints `[gate] VERDICT: PASS`. VERIFIED. Real-device framing (start/stop
  bits, parity, baud) is INFERRED per hardware until confirmed on target.
- **SOURCE**: CSS-Electronics CAN-to-DBC pipeline, inspiration, license UNKNOWN
  (ideas only); `cwe`; `binutils-docs` (hexdump-based capture review).
