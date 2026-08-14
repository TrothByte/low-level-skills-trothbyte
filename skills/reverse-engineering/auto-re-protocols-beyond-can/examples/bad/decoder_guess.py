"""Teaching example of the WRONG approach: guessing a protocol from one frame.

This decoder is deliberately bad. It commits the three sins the reference rules
target:
  1. Guessed field boundaries -- assumes frame = STX + len + 4-byte payload +
     checksum (7 bytes), while the real frames are 9 bytes with an 8-bit
     counter, a length field, a 16-bit sample counter and a 3-byte payload, so
     every boundary it prints is shifted.
  2. Endianness assumption -- reads the 16-bit field as BIG-endian (the true
     value is little-endian: bytes `01 00` are the value `0x0001`, not
     `0x0100`).
  3. Ignored verify gate -- its guessed checksum never matches, and instead of
     updating the model it dismisses every mismatch as "noise" and prints PASS
     unconditionally, including on the corrupted frames.

Compare its output with examples/good/run_pipeline.py on the same log: same
bytes, one confident wrong answer vs one evidence-based gated answer.

Usage: python decoder_guess.py [path/to/synthetic_uart.log]
"""

import sys
from pathlib import Path


def read_frames(path):
    frames = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line:
            frames.append([int(tok, 16) for tok in line.split()])
    return frames


def guessed_checksum(payload4):
    r = 0
    for b in payload4:
        r ^= b
    return r


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else (
        Path(__file__).resolve().parent.parent / "good" / "synthetic_uart.log")
    frames = read_frames(path)
    print("[guess] assumed frame (from one sample): STX(0x55) len payload[4B] cksum(1B)")
    print("[guess] endianness: big-endian (assumed, platform default)")
    for i, f in enumerate(frames[:5]):
        g_len = f[1]
        g_payload = f[2:6]
        g_status = (f[3] << 8) | f[4]
        g_cs = guessed_checksum(g_payload)
        if g_cs == f[6]:
            verdict = "PASS"
            note = ""
        else:
            # The bad pattern: explain the mismatch away instead of revising the model.
            verdict = "PASS"
            note = f" (cksum {g_cs:02x} != {f[6]:02x} -- dismissing as device noise)"
        print(f"[guess]   frame {i}: len=0x{g_len:02x} payload={' '.join(f'{b:02x}' for b in g_payload)} "
              f"status=0x{g_status:04x} -> {verdict}{note}")
    print(f"[guess] summary: decoded {len(frames)}/{len(frames)} frames, all PASS")
    print("[guess] protocol = STX + len + 4-byte payload + big-endian status + XOR checksum")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
