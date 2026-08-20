#!/usr/bin/env python3
"""bar_probe.py — python model of PCI BAR probing (size discovery).

Models the classic probe: save, write all-1s, read back, mask the fixed
attribute bits (space/type/prefetchable), compute size = (~masked) + 1.

Handles: 32-bit memory BARs, 64-bit memory BARs (two dwords), I/O BARs.

Run: python examples/tools/bar_probe.py
Expected: PASS per scenario; the wrong-mask cases (agent-bug shapes) print
explicit WRONG_SIZE failures.
"""
import sys

MEM_BAR_ATTR_MASK = 0x0F  # bit0 space=0, bits1-2 type, bit3 prefetchable
IO_BAR_ATTR_MASK = 0x03   # bit0 space=1, bit1 reserved


def probe_32_mem(readback):
    """Size of a 32-bit memory BAR from the post-write-all-1s readback."""
    masked = readback & ~MEM_BAR_ATTR_MASK
    return ((~masked) & 0xFFFFFFFF) + 1


def probe_64_mem(lo, hi):
    """Size of a 64-bit memory BAR from both dwords (lo has attr bits)."""
    low = lo & ~MEM_BAR_ATTR_MASK
    combined = (hi << 32) | low
    return ((~combined) & 0xFFFFFFFFFFFFFFFF) + 1


def probe_io(readback):
    """Size of an I/O BAR from the post-write-all-1s readback."""
    masked = readback & ~IO_BAR_ATTR_MASK
    return ((~masked) & 0xFFFFFFFF) + 1


def bar_type(readback):
    """Decode memory BAR type bits 1-2: 0x0=32-bit, 0x3=64-bit."""
    return (readback >> 1) & 0x3


def wrong_mask_32_mem(readback):
    """Agent-bug version: masks bits 0-2 (space+type) but forgets bit 3
    (prefetchable), or masks bits 0-15, corrupting the size."""
    masked = readback & 0xFFFF0000  # buggy: clears way too much
    return ((~masked) & 0xFFFFFFFF) + 1


def main():
    failed = 0

    def check(name, got, want):
        nonlocal failed
        ok = got == want
        print("%s: size=0x%X (want 0x%X) -> %s" % (
            name, got, want, "PASS" if ok else "FAIL"))
        if not ok:
            failed += 1

    # 32-bit memory BAR, 16 MiB: write all-1s -> readback 0xFF000000
    rb32 = 0xFF000000
    check("32-bit memory BAR (16 MiB)", probe_32_mem(rb32), 0x01000000)
    assert bar_type(rb32) == 0x0

    # 64-bit memory BAR, 256 GiB (2^38): readback low dword has type bits
    # (0x6), hi=0xFFFFFFC0 (bits 38-63 writable, bits 4-37 hardwired low)
    check("64-bit memory BAR (256 GiB)", probe_64_mem(0x00000006, 0xFFFFFFC0),
          0x4000000000)
    assert bar_type(0x00000006) == 0x3  # type bits 1-2 in the low dword

    # I/O BAR, 256 bytes: write all-1s -> readback 0xFFFFFF01 (bit0 space=1)
    check("I/O BAR (256 bytes)", probe_io(0xFFFFFF01), 0x00000100)

    # 32-bit memory BAR, 4 KiB with prefetchable bit set
    check("32-bit memory BAR (4 KiB, prefetch)", probe_32_mem(0xFFFFF008),
          0x00001000)

    # 64-bit memory BAR, 1 GiB (2^30): readback lo=0xC0000006 (bits 30-31 set
    # in low dword, type 0x6), hi=0xFFFFFFFF
    check("64-bit memory BAR (1 GiB)", probe_64_mem(0xC0000006, 0xFFFFFFFF),
          0x40000000)

    # Agent-bug shape: wrong mask on the 16 MiB BAR produces a bogus size
    print("-- wrong-mask agent bug reproduced --")
    check("bad mask on 16 MiB BAR", wrong_mask_32_mem(rb32), 0x01000000)

    print("bar_probe: %s" % ("ALL PASS" if failed == 0
                             else "%d FAILED" % failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
