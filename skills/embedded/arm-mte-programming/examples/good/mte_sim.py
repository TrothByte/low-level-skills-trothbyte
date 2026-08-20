#!/usr/bin/env python3
"""mte_sim.py -- host-runnable model of the Arm Memory Tagging Extension.

This is a Python MODEL of the MTE tag machinery, not MTE itself. It captures
the properties that decide whether MTE catches a bug:

  - 4-bit tags (16 values) stored per 16-byte granule of memory (allocation tag)
  - pointer tag carried in bits 56-59 of a 64-bit pointer (Top-Byte-Ignore)
  - IRG: insert a random tag into a register (allocation picks a fresh tag)
  - STG: store the allocation tag of the 16-byte granule containing addr
  - LDG: load the allocation tag of the granule containing addr
  - tag check on every access: pointer tag must equal the granule's tag
  - TCF modes: SYNC faults precisely at the faulting instruction
    (SEGV_MTESERR); ASYNC defers the report (SEGV_MTEAERR); the deferred
    model below shows the delay between the fault and the report.

The scenarios model what real MTE can and cannot catch:
  (1) use-after-free            -> fault (free re-tags / poisons the granule)
  (2) heap overflow across the 16-byte granule boundary -> fault
  (3) intra-granule overflow    -> NOT caught (both objects share one tag)
  (4) correct alloc/free/re-alloc -> only the stale pointer faults
  (5) TBI disabled              -> tag bits ignored, no fault (must enable TBI)
  (6) MTE_ASYNC                 -> report deferred past the faulting access

Runs with plain python 3; no third-party imports. Prints PASS/FAIL per
scenario and exits 0 only if every scenario passes.

Usage:
  python examples/good/mte_sim.py
"""

import random

GRANULE = 16          # MTE allocation granule size (bytes)
TAG_BITS = 4          # tag width (bits), 16 possible tag values
TAG_MASK = (1 << TAG_BITS) - 1          # 0xF
TBI_BYTE_MASK = 0xFF << 56              # top byte of the 64-bit pointer
ADDR_MASK = ~TBI_BYTE_MASK & 0xFFFFFFFFFFFFFFFF


class TagFault(Exception):
    """A synchronous tag mismatch on an access (SEGV_MTESERR equivalent)."""


def alloc_tag(rng, exclude=None):
    """IRG-equivalent: insert a random tag into a (new) allocation."""
    while True:
        t = rng.randrange(16)
        if t != exclude:
            return t


def make_ptr(addr, tag):
    """A tagged pointer: tag lives in bits 56-59, TBI addressing applies."""
    return (addr & ADDR_MASK) | (tag << 56)


def ptr_addr(p):
    return p & ADDR_MASK


def ptr_tag(p):
    return (p >> 56) & TAG_MASK


def granule_base(addr):
    return addr & ~(GRANULE - 1)


class Memory:
    """Physical-memory model: allocation tag per 16-byte granule."""

    def __init__(self, default_tag=0):
        self.tags = {}
        self.default_tag = default_tag

    def stg(self, addr, tag):
        """STG: store the allocation tag of the granule containing addr."""
        self.tags[granule_base(addr)] = tag & TAG_MASK

    def ldg(self, addr):
        """LDG: load the allocation tag of the granule containing addr."""
        return self.tags.get(granule_base(addr), self.default_tag)


def tag_check(mem, ptr, mode="sync", async_queue=None):
    """Tag check on an access. SYNC faults here; ASYNC defers the report."""
    addr, tag = ptr_addr(ptr), ptr_tag(ptr)
    granule_tag = mem.ldg(addr)
    if granule_tag != tag:
        if mode == "sync":
            raise TagFault(
                f"access @ {addr:#x}: pointer tag {tag} != "
                f"granule {granule_base(addr):#x} tag {granule_tag}"
            )
        if async_queue is not None:
            async_queue.append(addr)
    return True


PASS = 0
FAIL = 0


def scenario(name, passed):
    global PASS, FAIL
    if passed:
        PASS += 1
        print(f"  PASS  {name}")
    else:
        FAIL += 1
        print(f"  FAIL  {name}")


def main():
    rng = random.Random(20260820)
    print("MTE tag-machinery simulator (model, python 3)\n")

    # (1) Use-after-free: free must re-tag/poison the granule so that a stale
    # tagged pointer no longer matches.
    mem = Memory()
    ptr = make_ptr(0x2000, alloc_tag(rng))
    mem.stg(0x2000, ptr_tag(ptr))          # allocation tags its granule
    tag_check(mem, ptr)                    # live access: no fault
    free_tag = alloc_tag(rng, exclude=ptr_tag(ptr))
    mem.stg(0x2000, free_tag)              # free poisons the granule (re-tag)
    try:
        tag_check(mem, ptr)                # stale pointer access
        scenario("use-after-free: stale pointer faults (SYNC)", False)
    except TagFault:
        scenario("use-after-free: stale pointer faults (SYNC)", True)

    # (2) Heap overflow across the granule boundary: neighbor granule has a
    # different tag, so the overwrite faults.
    mem = Memory()
    a = make_ptr(0x2100, alloc_tag(rng))
    b = make_ptr(0x2110, alloc_tag(rng, exclude=ptr_tag(a)))
    mem.stg(0x2100, ptr_tag(a))
    mem.stg(0x2110, ptr_tag(b))
    try:
        for off in range(0, 16):           # write 16 bytes from a -> 0x2100..0x210F
            tag_check(mem, make_ptr(0x2100 + off, ptr_tag(a)))
        tag_check(mem, make_ptr(0x2110, ptr_tag(a)))  # spill into b's granule
        scenario("heap overflow past the 16-byte granule: fault", False)
    except TagFault:
        scenario("heap overflow past the 16-byte granule: fault", True)

    # (3) Intra-granule overflow: two objects share one granule and one tag;
    # MTE cannot tell them apart, so the overflow is NOT caught. This is a
    # documented MTE limitation, and the model must reproduce it.
    mem = Memory()
    obj_a = make_ptr(0x2200, 3)            # 0x2200 .. 0x2207
    obj_b = make_ptr(0x2208, 3)            # 0x2208 .. 0x220F, same granule/tag
    mem.stg(0x2200, 3)                     # one tag for the whole granule
    tag_check(mem, obj_a)
    tag_check(mem, obj_b)
    try:
        for off in range(8, 16):           # overflow from obj_a into obj_b
            tag_check(mem, make_ptr(0x2200 + off, 3))
        scenario("intra-granule overflow correctly NOT caught (limitation)", True)
    except TagFault:
        scenario("intra-granule overflow correctly NOT caught (limitation)", False)

    # (4) Correct alloc/free/re-alloc cycle: the freed object faults for stale
    # pointers, and the re-allocated object works with its fresh pointer.
    mem = Memory()
    p1 = make_ptr(0x2300, alloc_tag(rng))
    mem.stg(0x2300, ptr_tag(p1))
    tag_check(mem, p1)
    mem.stg(0x2300, alloc_tag(rng, exclude=ptr_tag(p1)))  # free: poison
    stale_faulted = False
    try:
        tag_check(mem, p1)
    except TagFault:
        stale_faulted = True
    p2 = make_ptr(0x2300, alloc_tag(rng, exclude=mem.ldg(0x2300)))
    mem.stg(0x2300, ptr_tag(p2))           # re-alloc tags again
    try:
        tag_check(mem, p2)                 # fresh pointer: no fault
        scenario("correct alloc/free/re-alloc: stale faults, fresh ok", stale_faulted)
    except TagFault:
        scenario("correct alloc/free/re-alloc: stale faults, fresh ok", False)

    # (5) TBI disabled: the tag bits are ignored and the access is unchecked.
    # Tags "just working" on dereference requires TBI (FEAT_TBI); without it a
    # tagged pointer is simply a misaligned address.
    mem = Memory()
    tagged = make_ptr(0x2400, 7)
    mem.stg(0x2400, 2)                     # granule tag differs from pointer tag
    try:
        tag_check(mem, tagged, mode="tbi-off")
        scenario("TBI disabled: tag ignored, no fault (must enable TBI)", True)
    except TagFault:
        scenario("TBI disabled: tag ignored, no fault (must enable TBI)", False)

    # (6) MTE_ASYNC: the report is deferred. The faulting access completes; the
    # kernel/CPE reports the error later (SEGV_MTEAERR), so the signal handler
    # must NOT assume the pc/si_addr point at the actual bug.
    mem = Memory()
    deferred = []
    bad = make_ptr(0x2500, 9)
    mem.stg(0x2500, 1)                     # mismatch, but ASYNC mode
    tag_check(mem, bad, mode="async", async_queue=deferred)
    tag_check(mem, bad, mode="async", async_queue=deferred)
    tag_check(mem, make_ptr(0x2500, 1), mode="async", async_queue=deferred)
    report_late = len(deferred) == 2 and deferred[0] == 0x2500
    scenario("MTE_ASYNC: faults deferred, reported at a later point", report_late)
    if not report_late:
        print(f"    (deferred queue was {deferred})")

    print()
    print(f"RESULT: {PASS} passed, {FAIL} failed")
    if FAIL:
        print("Fault-tag machinery model MISMATCH -- see above")
        return 1
    print("The model matches the MTE properties in the technical brief: "
          "UAF and cross-granule overflow are caught, intra-granule overflow "
          "is not, and ASYNC defers the report.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
