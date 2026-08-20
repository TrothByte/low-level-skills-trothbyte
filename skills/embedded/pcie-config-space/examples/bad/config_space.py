#!/usr/bin/env python3
"""config_space.py — BUGGY host model of PCIe config space (agent-bug class).

Deliberate defects the skill's reasoning must reject:
  1. Capabilities read at FIXED offsets, not via next pointers.
  2. BAR probe clears bits 0-15 (too many width bits) -> wrong size.
  3. 64-bit BAR (type 0x3) treated as 32-bit.
  4. MSI-X table offset decoded from the WRONG BAR (BIR ignored).
  5. MSI-X page alignment ignored (8-byte alignment only).
  6. Byte-order bug: vendor+device packed into one 32-bit read.

Run: python examples/bad/config_space.py
Expected: the buggy values are printed; the script exits 0 (bugs are silent).
"""
import sys


def read_bar_wrong_mask(readback):
    """Clears bits 0-15 instead of 0-3: a 4 KiB BAR (readback 0xFFFFF008)
    computes as ~0xFFFFF008 & 0xFFFF0000 -> size 0x10000 (64 KiB) instead of
    0x1000 (4 KiB)."""
    m = readback & 0xFFFF0000
    return (~m & 0xFFFFFFFF) + 1


def read_bar32(readback):
    """'Correct-looking' but 32-bit-only probe; fails silently on 64-bit."""
    m = readback & ~0x0F
    return (~m & 0xFFFFFFFF) + 1


def main():
    # vendor 0x8086 device 0x100E
    vid_did_bug = 0x100E8086  # byte-order bug: single 32-bit read
    print("vendor+device packed as one dword: 0x%08X (BUG: two 16-bit "
          "words needed)" % vid_did_bug)

    # BAR probe with wrong mask: real BAR is 4 KiB (readback 0xFFFFF008 with
    # prefetchable bit set) -> wrong mask reports 64 KiB
    size = read_bar_wrong_mask(0xFFFFF008)
    print("BAR0 size (wrong mask 0xFFFF0000): 0x%X (BUG: expected 0x1000)"
          % size)

    # 64-bit BAR type 0x3 treated as 32-bit: readback lo=0x00000006 -> the
    # low dword alone yields a truncated size
    lo = 0x00000006
    print("64-bit BAR treated as 32-bit: size=0x%X (BUG: type bits 1-2 == 0x3 "
          "mean 64-bit, second dword required)" % read_bar32(lo))

    # MSI-X: table BIR=2, offset 0x1000, but the buggy decoder reads BAR0
    # (BIR ignored) and only checks 8-byte alignment, never page alignment
    table_field = (2) | 0x1000  # BIR=2, offset 0x1000 (page-aligned)
    pba_field = (2) | 0x2000
    bir = table_field & 0x7
    off = table_field & 0xFFFFF8
    print("MSI-X: BIR=%d table_off=0x%04X (decoder assumes BAR0 — "
          "BUG: BIR ignored)" % (bir, off))

    unaligned = 0x1008  # 8B aligned, NOT page aligned
    print("MSI-X table 0x%04X: 8B-aligned=%s page-aligned=%s (BUG: page "
          "alignment not enforced)" % (
              unaligned, (unaligned & 0x7) == 0, (unaligned & 0xFFF) == 0))

    print("NOTE: script exits 0 — these bugs are silent and must be caught "
          "by review")
    return 0


if __name__ == "__main__":
    sys.exit(main())
