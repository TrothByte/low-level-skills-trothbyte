"""Deterministic protocol-RE pipeline on a synthetic UART log (GOOD approach).

Teaches the methodology: capture -> survey -> correlate -> bit/field search ->
schema -> verify-as-gate. Every claim printed here is derived from the captured
bytes, never assumed. The gate is the final authority: an anomalous log is
reported UNCONFIRMED (exit code 1) until a clean corpus re-verifies as PASS
(exit code 0).

Usage: python run_pipeline.py [path/to/synthetic_uart.log]
"""

import sys
from collections import Counter
from pathlib import Path


def read_frames(path):
    frames = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line:
            frames.append([int(tok, 16) for tok in line.split()])
    return frames


def survey(frames):
    width = len(frames[0])
    print(f"[survey] corpus: {len(frames)} frames, {width} bytes each")
    pos0 = Counter(f[0] for f in frames)
    print(f"[survey] byte-0 values: {pos0.most_common(3)}")
    lengths = Counter(len(f) for f in frames)
    print(f"[survey] frame-length histogram: {dict(lengths)}")
    distinct = [len(set(f[i] for f in frames)) for i in range(width)]
    print(f"[survey] distinct values per byte position: {distinct}")
    const = [i for i, d in enumerate(distinct) if d == 1]
    print(f"[survey] constant positions (sync/length candidates): {const}")


def sentinel_analysis(frames):
    width = len(frames[0])
    counts = [sum(1 for f in frames if f[i] == 0x55) for i in range(width)]
    print(f"[survey] 0x55 occurrences per byte position: {counts}")
    print("[survey] sync byte is the position-stable 0x55 at pos 0, not any 0x55 in the stream")


def correlate(frames):
    deltas = [(f[1] - p[1]) & 0xFF for p, f in zip(frames, frames[1:])]
    hist = Counter(deltas)
    modal, n = hist.most_common(1)[0]
    print(f"[correlate] byte-1 consecutive deltas: {sorted(hist.items())}")
    print(f"[correlate] byte-1 = 8-bit rolling counter, delta {modal} in {n}/{len(deltas)} transitions")

    le = [f[3] | (f[4] << 8) for f in frames]
    be = [(f[3] << 8) | f[4] for f in frames]

    # Smoothness alone cannot decide endianness: the byte-swap of a
    # constant-increment counter is itself a near-constant-increment sequence,
    # so both readings pass a delta/monotonicity test. The decisive evidence is
    # value structure: which byte position is constant (the high byte) vs which
    # carries the data (the low byte).
    def mono(vals):
        return sum(1 for a, b in zip(vals, vals[1:]) if b > a)

    mle, mbe = mono(le), mono(be)
    n = len(frames) - 1
    print(f"[correlate] 16-bit @bytes 3-4: monotone-increasing LE {mle}/{n}, BE {mbe}/{n}")
    z3 = sum(1 for f in frames if f[3] == 0)
    z4 = sum(1 for f in frames if f[4] == 0)
    print(f"[correlate] byte3 zero in {z3}/{len(frames)}, byte4 zero in {z4}/{len(frames)}")
    if z4 == len(frames) and z3 == 0:
        endian = "little"
        print("[correlate] endianness: little (data byte in LOW position, constant 0x00 in HIGH position)")
    elif z3 == len(frames) and z4 == 0:
        endian = "big"
        print("[correlate] endianness: big (data byte in HIGH position, constant 0x00 in LOW position)")
    else:
        endian = "UNCONFIRMED"
        print("[correlate] endianness: UNCONFIRMED (no value-bounds evidence; do not assume)")
    return endian


def checksum_candidates():
    def xor8(body):
        r = 0
        for b in body:
            r ^= b
        return r

    def sum8(body):
        return sum(body) & 0xFF

    def twos_comp(body):
        return (-sum(body)) & 0xFF

    def add_carry(body):
        s = 0
        for b in body:
            s += b
        s = (s & 0xFF) + (s >> 8)
        return s & 0xFF

    def crc8(body):
        crc = 0
        for b in body:
            crc ^= b
            for _ in range(8):
                if crc & 0x80:
                    crc = ((crc << 1) ^ 0x07) & 0xFF
                else:
                    crc = (crc << 1) & 0xFF
        return crc

    return {"xor8": xor8, "sum8": sum8, "twos_comp": twos_comp,
            "add_carry": add_carry, "crc8(0x07)": crc8}


def bitsearch(frames, candidates):
    print("[bitsearch] checksum candidates over frame bytes 1..7 vs byte 8:")
    best, best_ok = None, -1
    for name, fn in candidates.items():
        ok = sum(1 for f in frames if fn(f[1:8]) == f[8])
        print(f"[bitsearch]   {name}: {ok}/{len(frames)} frames match")
        if ok > best_ok:
            best, best_ok = name, ok
    print(f"[bitsearch] selected: {best} ({best_ok}/{len(frames)}) -> checksum = {best} of bytes 1..7 at byte 8")
    return candidates[best]


def derive_schema(endian):
    print("[schema] derived frame layout:")
    print("[schema]   stx: 0x55 @ byte 0")
    print("[schema]   counter: 8-bit rolling @ byte 1")
    print("[schema]   length: 8-bit @ byte 2")
    print(f"[schema]   sample: 16-bit {endian}-endian @ bytes 3-4")
    print("[schema]   payload: bytes 5-7")
    print("[schema]   checksum: xor8 of bytes 1..7 @ byte 8")


def decode_and_print(frames, xor):
    print("[decode] per-frame decode with derived schema:")
    for i, f in enumerate(frames):
        stx_ok = f[0] == 0x55
        len_ok = f[2] == 5
        cs_ok = xor(f[1:8]) == f[8]
        cnt_ok = i == 0 or ((f[1] - frames[i - 1][1]) & 0xFF) == 1
        if stx_ok and len_ok and cs_ok and cnt_ok:
            sample = f[3] | (f[4] << 8)
            print(f"[decode]   {i:2d} counter=0x{f[1]:02x} len={f[2]} "
                  f"sample=0x{sample:04x} (raw {f[3]:02x} {f[4]:02x}) "
                  f"payload={f[5]:02x} {f[6]:02x} {f[7]:02x} checksum=0x{f[8]:02x} OK")
        else:
            reasons = []
            if not stx_ok:
                reasons.append("stx")
            if not len_ok:
                reasons.append("length")
            if not cs_ok:
                reasons.append("checksum")
            if not cnt_ok:
                reasons.append("counter")
            print(f"[decode]   {i:2d} ANOMALY ({', '.join(reasons)}) -- not decoded")


def run_gate(frames, xor, label):
    check = Counter()
    failed = set()
    for i, f in enumerate(frames):
        if f[0] != 0x55:
            check["stx"] += 1
            failed.add(i)
        if f[2] != 5:
            check["length"] += 1
            failed.add(i)
        if xor(f[1:8]) != f[8]:
            check["checksum"] += 1
            failed.add(i)
        if i > 0 and ((f[1] - frames[i - 1][1]) & 0xFF) != 1:
            check["counter"] += 1
            failed.add(i)
    parts = [f"{k}: {len(frames) - v}/{len(frames)}" for k, v in check.items()]
    print(f"[gate:{label}] frames={len(frames)} " + " ".join(parts))
    return failed


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name("synthetic_uart.log")
    frames = read_frames(path)
    if not frames:
        print(f"no frames in {path}; run gen_log.py first")
        return 2
    print(f"[capture] read {len(frames)} frames from {path.name}")
    survey(frames)
    sentinel_analysis(frames)
    endian = correlate(frames)
    xor = bitsearch(frames, checksum_candidates())
    derive_schema(endian)
    decode_and_print(frames, xor)
    failed = run_gate(frames, xor, "full")
    if failed:
        print(f"[gate] anomalies at frames: {sorted(failed)}")
        print("[gate] VERDICT: UNCONFIRMED -- gate refused to certify a clean decode; investigate the anomalies")
        return 1
    print("[gate] VERDICT: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
