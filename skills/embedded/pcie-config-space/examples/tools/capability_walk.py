#!/usr/bin/env python3
"""capability_walk.py — python model of the PCI/PCIe capability linked-list walk.

Given a synthetic config-space blob (256-byte legacy space + optionally
extended), walk the capability list starting from the Capabilities Pointer at
0x34 (header type 0/1). Decode each capability ID and its location; validate
termination (next == 0) and detect cycles (infinite-loop bug class).

Run: python examples/tools/capability_walk.py
Expected: good blob -> PASS, cyclic blob -> FLAG.
"""
import sys

CAP_IDS = {
    0x01: "PM",
    0x03: "VPD",
    0x05: "MSI",
    0x06: "PCI-X",
    0x0A: "MSI-X",
    0x10: "PCIe",
    0x11: "MSI-X",
    0x19: "LTR",
    0x1A: "ACS",
}


class WalkResult:
    OK = "OK"
    TERMINATED_ZERO = "terminated"
    CYCLE = "CYCLE"
    OOB = "OUT_OF_RANGE"
    NONE_FOUND = "NO_CAPABILITIES"


def walk_caps(blob, caps_ptr_offset=0x34, max_iter=256):
    """Walk the capability linked list in a config-space blob.

    blob: bytearray/bytes of at least caps_ptr_offset+1 length.
    Returns (list_of_caps, status). Each cap is (offset, id, name).
    """
    if len(blob) < caps_ptr_offset + 1:
        return [], WalkResult.OOB
    start = blob[caps_ptr_offset]
    if start == 0:
        return [], WalkResult.NONE_FOUND

    caps = []
    seen = set()
    off = start
    for _ in range(max_iter):
        if off == 0:
            return caps, WalkResult.TERMINATED_ZERO
        if off in seen:
            return caps, WalkResult.CYCLE
        if off + 1 >= len(blob):
            return caps, WalkResult.OOB
        seen.add(off)
        cap_id = blob[off]
        nxt = blob[off + 1]
        caps.append((off, cap_id, CAP_IDS.get(cap_id, "?")))
        off = nxt
    return caps, WalkResult.CYCLE  # hit iteration cap -> infinite loop shape


def build_good_blob():
    """Synthetic config space: caps ptr -> PM (0x40) -> MSI (0x50) -> PCIe
    (0x60) -> MSI-X (0x70) -> end (next=0)."""
    blob = bytearray(4096)  # full ECAM-sized space
    blob[0x34] = 0x40
    blob[0x40] = 0x01; blob[0x41] = 0x50  # PM
    blob[0x50] = 0x05; blob[0x51] = 0x60  # MSI
    blob[0x60] = 0x10; blob[0x61] = 0x70  # PCIe
    blob[0x70] = 0x11; blob[0x71] = 0x00  # MSI-X, terminates
    return blob


def build_cycle_blob():
    """Buggy blob: PM (0x40) -> MSI (0x50) -> PCIe (0x60) -> back to 0x40.
    An agent that follows pointers but lacks cycle detection loops forever."""
    blob = bytearray(4096)
    blob[0x34] = 0x40
    blob[0x40] = 0x01; blob[0x41] = 0x50  # PM
    blob[0x50] = 0x05; blob[0x51] = 0x60  # MSI
    blob[0x60] = 0x10; blob[0x61] = 0x40  # PCIe -> cycle back to 0x40
    return blob


def build_oob_blob():
    """Buggy blob: a 0x42-byte config region whose PM next pointer (0x43)
    runs past the end — an agent that does not bounds-check reads garbage."""
    blob = bytearray(0x42)  # truncated config space (e.g. legacy 66-byte cut)
    blob[0x34] = 0x40
    blob[0x40] = 0x01; blob[0x41] = 0x43  # PM -> next 0x43 (out of range)
    return blob


def fmt(caps):
    return " -> ".join(
        "0x%02X/%s" % (off, name) for off, _id, name in caps
    )


def main():
    failed = 0

    good = build_good_blob()
    caps, status = walk_caps(good)
    expect = [(0x40, 0x01, "PM"), (0x50, 0x05, "MSI"),
              (0x60, 0x10, "PCIe"), (0x70, 0x11, "MSI-X")]
    if status == WalkResult.TERMINATED_ZERO and caps == expect:
        print("PASS good blob: %s (%d caps, terminated at next=0)" % (
            fmt(caps), len(caps)))
    else:
        print("FAIL good blob: status=%s caps=%s" % (status, caps))
        failed += 1

    cycle = build_cycle_blob()
    caps, status = walk_caps(cycle)
    if status == WalkResult.CYCLE:
        print("FLAG cyclic blob: cycle detected (visited %s)" % fmt(caps))
    else:
        print("FAIL cyclic blob: expected CYCLE, got %s" % status)
        failed += 1

    oob = build_oob_blob()
    caps, status = walk_caps(oob)
    if status == WalkResult.OOB:
        print("FLAG out-of-range blob: next pointer outside config space")
    else:
        print("FAIL oob blob: expected OUT_OF_RANGE, got %s" % status)
        failed += 1

    empty = bytearray(4096)
    caps, status = walk_caps(empty)
    if status == WalkResult.NONE_FOUND:
        print("PASS empty blob: no capabilities (ptr=0)")
    else:
        print("FAIL empty blob: expected NO_CAPABILITIES, got %s" % status)
        failed += 1

    print("capability_walk: %s" % ("ALL PASS" if failed == 0
                                   else "%d FAILED" % failed))
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
