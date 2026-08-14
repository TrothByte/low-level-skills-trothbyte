"""Generate the synthetic UART protocol log used by the good and bad examples.

Teaching ground truth (NOT known to the decoder in advance):
  byte 0  STX            = 0x55
  byte 1  frame counter  = 8-bit rolling, +1 per frame, wraps 0xFF -> 0x00
  byte 2  payload length = 5 (constant)
  byte 3-4 sample index  = 16-bit little-endian, +4 per frame, small bounded
                           value so the HIGH byte is always 0x00 -- this zero
                           structure is the endianness evidence (see run_pipeline)
  byte 5-7 payload       = 3 sensor bytes (may contain 0x55 -- sentinel masking)
  byte 8  checksum       = XOR over bytes 1..7

Two anomalies are injected (line noise, not protocol features):
  frame 19: payload byte flipped without recomputing the checksum
  frame 37: counter skipped by 2 (checksum recomputed, so it is checksum-valid
            but breaks counter continuity)

Usage:
  python gen_log.py                          -> synthetic_uart.log (anomalies)
  python gen_log.py --clean <path>           -> anomaly-free log
  python gen_log.py <path>                   -> log with anomalies
"""

import sys
from pathlib import Path

STX = 0x55
FRAMES = 60
SAMPLE_DELTA = 0x0004
SAMPLE_START = 0x0001
CHECKSUM_ANOMALY = 19
COUNTER_ANOMALY = 37


def build_frame(i, clean=False):
    counter = i & 0xFF
    if not clean and i == COUNTER_ANOMALY:
        counter = (i + 2) & 0xFF
    length = 5
    sample = (SAMPLE_START + i * SAMPLE_DELTA) & 0xFFFF
    payload = bytes([0x30 + i % 10, 0x40 + i % 5, 0x50 + i % 7])
    body = bytes([counter, length, sample & 0xFF, sample >> 8]) + payload
    cs = 0
    for b in body:
        cs ^= b
    frame = [STX] + list(body) + [cs]
    if not clean and i == CHECKSUM_ANOMALY:
        frame[6] ^= 0x44  # flip a payload bit; checksum NOT recomputed
    return frame


def fmt(frame):
    return " ".join(f"{b:02x}" for b in frame)


def main():
    args = sys.argv[1:]
    clean = False
    if args and args[0] == "--clean":
        clean = True
        args = args[1:]
    out = Path(args[0]) if args else Path(__file__).with_name("synthetic_uart.log")
    lines = [fmt(build_frame(i, clean)) for i in range(FRAMES)]
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    tag = "clean" if clean else "2 anomalies"
    print(f"wrote {len(lines)} frames ({tag}) to {out}")


if __name__ == "__main__":
    main()
