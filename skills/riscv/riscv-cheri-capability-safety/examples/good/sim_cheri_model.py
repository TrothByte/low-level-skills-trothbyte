#!/usr/bin/env python3
"""Self-contained model of CHERI capability enforcement (bounds, permissions,
tag). Models the CHERI capability model from cheri-spec §2/§6, NOT the ISA or
QEMU. The examples it checks are the good/bad .c patterns translated to the
model: an access faults iff the address is outside bounds OR a required
permission is missing OR the tag is clear.

Purpose: demonstrate WHY the bad patterns fail and the good ones pass, so the
review rules have a reproducible oracle. No QEMU-CHERI / cheribuild on this
machine (documented target: cheribuild run-sdk --cheribsd -- purecap-cc).
"""

TAG_OK = True


class Capability:
    def __init__(self, addr, base, length, perms, tag=TAG_OK):
        self.addr = addr
        self.base = base
        self.length = length
        self.perms = set(perms)
        self.tag = tag

    def address_get(self):
        return self.addr

    def bounds_set(self, length):
        # narrow bounds to cover only `length` bytes from the address
        return Capability(self.addr, self.addr, length, self.perms)

    def offset_set(self, off):
        return Capability(self.addr + off, self.addr + off, self.length - off,
                          self.perms)

    def add(self, delta):
        # pointer arithmetic: escapes bounds -> tag cleared
        if not (self.base <= self.addr + delta < self.base + self.length):
            c = Capability(self.addr + delta, self.base, self.length, self.perms)
            c.tag = False
            return c
        return Capability(self.addr + delta, self.base, self.length, self.perms)


class Fault(Exception):
    pass


def load(cap, width, need=("load",)):
    if not cap.tag:
        raise Fault("tag fault: capability tag cleared")
    if not (cap.base <= cap.addr <= cap.base + cap.length - width):
        raise Fault("bounds fault: access outside capability bounds")
    missing = set(need) - cap.perms
    if missing:
        raise Fault(f"permission fault: missing {sorted(missing)}")


def store(cap, width, need=("store",)):
    if not cap.tag:
        raise Fault("tag fault: capability tag cleared")
    if not (cap.base <= cap.addr <= cap.base + cap.length - width):
        raise Fault("bounds fault: store outside capability bounds")
    missing = set(need) - cap.perms
    if missing:
        raise Fault(f"permission fault: missing {sorted(missing)}")


def check(name, fn, expect_fault):
    try:
        fn()
        ok = not expect_fault
    except Fault as e:
        ok = bool(expect_fault)
        if expect_fault:
            print(f"  {name}: PASS (faults as expected: {e})")
        else:
            print(f"  {name}: FAIL (unexpected fault: {e})")
    else:
        print(f"  {name}: {'PASS' if ok else 'FAIL (no fault, expected one)'}")
    return ok


def main():
    print("CHERI capability model — bounds/permission/tag enforcement\n")

    ok = True
    perms_all = {"load", "store", "load_cap", "store_cap", "execute"}

    # bad_oob_derived: p + (n+1) escapes bounds -> tag cleared
    def bad_oob():
        buf = Capability(0x1000, 0x1000, 64, perms_all)
        tail = buf.add(17 * 4)            # past 16-element object
        load(tail, 4)
    ok &= check("bad_oob_derived (arithmetic past bounds)", bad_oob, True)

    # bad_int_roundtrip: integer round-trip -> tag cleared
    def bad_int():
        buf = Capability(0x2000, 0x2000, 64, perms_all)
        back = Capability(buf.address_get(), 0x2000, 64, perms_all)
        back.tag = False                  # (uintptr_t) cast drops the tag
        store(back, 4)
    ok &= check("bad_int_roundtrip (tag dropped)", bad_int, True)

    # bad_byte_copy: memcpy payload -> tag cleared
    def bad_byte():
        src = Capability(0x3000, 0x3000, 64, perms_all)
        dst = Capability(src.addr, src.base, src.length, src.perms)
        dst.tag = False                   # byte-wise copy loses the tag
        load(dst, 4)
    ok &= check("bad_byte_copy (memcpy drops tag)", bad_byte, True)

    # bad_missing_perm: no store_cap on a capability store
    def bad_perm():
        cap = Capability(0x4000, 0x4000, 64, {"load", "store"})
        store(cap, 16, need=("store_cap",))
    ok &= check("bad_missing_perm (missing store_cap)", bad_perm, True)

    # good_bounded: bounds_set + offset_set within bounds -> no fault
    def good_bounded():
        obj = Capability(0x5000, 0x5000, 64, perms_all)
        values = obj.bounds_set(32)       # 8 * sizeof(int)
        p = values.offset_set(0)
        for i in range(8):
            store(p, 4)
            p = p.add(4)
    ok &= check("good_bounded (in-bounds sub-pointer)", good_bounded, False)

    # good_seal_copy: element-wise copy preserves tag
    def good_copy():
        src = Capability(0x6000, 0x6000, 64, perms_all)
        dst = Capability(src.addr, src.base, src.length, set(src.perms),
                         TAG_OK)          # element-wise copy keeps the tag
        load(dst, 4)
    ok &= check("good_seal_copy (element-wise copy keeps tag)", good_copy, False)

    print("\nAll model checks:", "PASS" if ok else "FAIL")
    print("Model of CHERI capability semantics (cheri-spec §2/§6) — not ISA/")
    print("hardware. Documented target: cheribuild run-sdk --cheribsd -- "
          "purecap-cc; QEMU-CHERI.")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
