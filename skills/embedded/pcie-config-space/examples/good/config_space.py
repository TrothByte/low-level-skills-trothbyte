#!/usr/bin/env python3
"""config_space.py — CORRECT host model of PCIe config space.

Builds a synthetic 4KB ECAM blob, decodes the type-0 header, walks the
capability linked list via next pointers, probes BAR sizes with the correct
width-bit mask, and decodes MSI-X with BIR + 8-byte/page alignment checks.

Run: python examples/good/config_space.py
Expected: PASS for every check.
"""
import sys

CAP_IDS = {0x01: "PM", 0x05: "MSI", 0x10: "PCIe", 0x11: "MSI-X", 0x19: "LTR"}


def build_blob():
    """Synthetic config: 8086:100E, class 0x02, BAR0 32-bit 16 MiB,
    caps PM@0x40 -> MSI@0x50 -> PCIe@0x60 -> MSI-X@0x70 -> end."""
    b = bytearray(4096)
    b[0x00:0x02] = bytes((0x86, 0x80))      # vendor 0x8086
    b[0x02:0x04] = bytes((0x0E, 0x10))      # device 0x100E
    b[0x09:0x0C] = bytes((0x00, 0x00, 0x02))  # prog-if/subclass/base
    b[0x0E] = 0x00                          # header type 0
    b[0x10:0x14] = bytes((0x00, 0x00, 0x00, 0xE0))  # BAR0 base 0xE0000000
    b[0x34] = 0x40                          # caps pointer
    b[0x40:0x42] = (0x01, 0x50)             # PM -> 0x50
    b[0x50:0x52] = (0x05, 0x60)             # MSI -> 0x60
    b[0x52:0x54] = (0x80, 0x00)             # MSI ctrl: 64-bit capable
    b[0x54:0x58] = (0x00, 0x00, 0xE0, 0xFE)  # addr_lo 0xFEE00000
    b[0x58:0x5C] = (0x00, 0x00, 0x00, 0x00)  # addr_hi
    b[0x5C:0x5E] = (0x33, 0x00)             # data 0x0033
    b[0x60:0x62] = (0x10, 0x70)             # PCIe -> 0x70
    b[0x70:0x72] = (0x11, 0x00)             # MSI-X, terminates
    b[0x72:0x74] = (0x03, 0x00)             # MSI-X ctrl: table size 3 (4 vec)
    b[0x74:0x78] = (0x00, 0x10, 0x00, 0x00)  # table @ BAR0 + 0x1000, BIR 0
    b[0x78:0x7C] = (0x00, 0x20, 0x00, 0x00)  # PBA @ BAR0 + 0x2000, BIR 0
    return b


def walk_caps(blob):
    """Correct walk: follow next pointers, terminate at 0, detect cycles."""
    start = blob[0x34]
    caps = []
    seen = set()
    off = start
    while off != 0:
        if off in seen:
            raise ValueError("capability cycle at 0x%02X" % off)
        if off + 1 >= len(blob):
            raise ValueError("capability out of range at 0x%02X" % off)
        seen.add(off)
        caps.append((off, blob[off], blob[off + 1]))
        off = blob[off + 1]
    return caps


def probe_bar32(readback):
    """Correct BAR probe: clear width bits 0-3, invert, +1."""
    m = readback & ~0x0F
    return (~m & 0xFFFFFFFF) + 1


def probe_bar64(lo, hi):
    low = lo & ~0x0F
    combined = (hi << 32) | low
    return (~combined & 0xFFFFFFFFFFFFFFFF) + 1


def check(name, got, want):
    ok = got == want
    print("%s: %s -> %s" % (name, got, "PASS" if ok else "FAIL"))
    return ok


def main():
    failed = 0
    b = build_blob()

    failed += not check("vendor ID", "%04X" % (b[0x01] << 8 | b[0x00]),
                        "8086")
    failed += not check("device ID", "%04X" % (b[0x03] << 8 | b[0x02]),
                        "100E")
    failed += not check("class code (base@high byte)",
                        "%02X:%02X:%02X" % (b[0x0B], b[0x0A], b[0x09]),
                        "02:00:00")

    caps = walk_caps(b)
    chain = " -> ".join("%s@0x%02X" % (CAP_IDS[i], o) for o, i, _ in caps)
    failed += not check("capability walk", chain,
                        "PM@0x40 -> MSI@0x50 -> PCIe@0x60 -> MSI-X@0x70")
    failed += not check("list termination",
                        caps[-1][2], 0)

    # MSI decode
    msi = b[0x50:0x60]
    is64 = (msi[2] | msi[3] << 8) & 0x0080
    addr = msi[4] | msi[5] << 8 | msi[6] << 16 | msi[7] << 24
    failed += not check("MSI 64-bit flag", bool(is64), True)
    failed += not check("MSI addr_lo", "0x%08X" % addr, "0xFEE00000")

    # MSI-X decode with alignment
    mx = b[0x70:0x7C]
    nvec = ((mx[2] | mx[3] << 8) & 0x07FF) + 1
    table = mx[4] | mx[5] << 8 | mx[6] << 16 | mx[7] << 24
    bir = table & 0x7
    off = table & 0xFFFFF8
    failed += not check("MSI-X vector count", nvec, 4)
    failed += not check("MSI-X BIR", bir, 0)
    failed += not check("MSI-X table offset", "0x%04X" % off, "0x1000")
    failed += not check("MSI-X table 8B aligned", off & 0x7 == 0, True)
    failed += not check("MSI-X table page aligned", off & 0xFFF == 0, True)

    # BAR probing
    failed += not check("BAR0 probe 16 MiB", hex(probe_bar32(0xFF000000)),
                        hex(0x1000000))
    failed += not check("BAR 64-bit 256 GiB",
                        hex(probe_bar64(0x00000006, 0xFFFFFFC0)),
                        hex(0x4000000000))

    print("config_space (good): %s" % ("ALL PASS" if not failed
                                       else "%d FAILED" % failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
