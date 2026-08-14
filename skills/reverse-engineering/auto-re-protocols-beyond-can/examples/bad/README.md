# Bad methodology — guessing a protocol instead of deriving it

`decoder_guess.py` runs against the SAME `synthetic_uart.log` as the good
pipeline. It is the three failure modes the reference rules target, each with
the observed symptom.

## 1. Guessed field boundaries

It assumes `STX + len + 4-byte payload + checksum`. The real frames are 9 bytes
(8-bit counter, length, 16-bit LE sample counter, 3-byte payload, checksum), so
every boundary it prints is shifted by one or more bytes: the counter and the
length field are reported as payload, and the real payload byte 6 is reported
as the checksum.

Symptom: `len=0x00` (the counter) and `payload=05 01 2c 30` (the length field
plus half the sample counter) — nonsense that the decoder never revises.

## 2. Endianness assumption

It reads the 16-bit field as big-endian: raw bytes `01 00` are printed as
`status=0x0100`. The true little-endian value is `0x0001` (the good pipeline
derives LE from the constant-byte position: byte 4 is `0x00` in every frame,
so it is the high byte). The bad decoder does not test the other
interpretation.

## 3. Ignored verify gate

Its guessed checksum never matches, and instead of updating the model it
prints `PASS` unconditionally, dismissing every mismatch as "device noise". The
corrupted frame 19 and the counter-skip frame 37 are decoded "successfully"
with no anomaly reported. There is no gate: the confidence of the summary
(`decoded 60/60 frames, all PASS`) is unrelated to correctness.

## Contrast

Good pipeline on the same log: `VERDICT: UNCONFIRMED`, anomalies at frames
19/37/38, exit code 1. Bad decoder on the same log: `all PASS`, exit code 0.
Same bytes — the difference is the method.
