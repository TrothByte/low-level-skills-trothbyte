#!/usr/bin/env python3
"""GOOD: python model of CO-RE (Compile-Once, Run-Everywhere) field-offset
relocation. Plain python 3.11, no dependencies. Run:

    python examples/good/core_reloc_model.py

Two kernel struct layouts (v1, v2) model two kernel versions. In v2 the field
task_struct.mm MOVED (offset 16 -> 24) and rss_stat.count was RENAMED to
rss_stat.stat. A real CO-RE program records NAME-based relocations (field
paths), and libbpf resolves the offsets against the target kernel's BTF at
load time. This model plays that role: offsets come from the target version's
layout, never from the compiler's machine.

We demonstrate four access patterns per kernel version and check each against
ground truth. Only patterns 3 and 4 fail on v2, and that failure is the whole
point: a correct checker must flag them as non-portable.
"""

import struct

SZ = {"u64": 8, "u32": 4, "ptr": 8}

# field lists: (name, type, child struct)  -- child struct for pointer/struct
# fields, None for scalars. Offsets are computed by accumulation (the model
# deliberately ignores C alignment padding; the principle is identical).
LAYOUT = {
    "v1": {
        # mm at offset 16
        "task_struct": [("state", "u64", None), ("pid", "u64", None), ("mm", "ptr", "mm_struct")],
        "mm_struct": [("mmap_base", "u64", None), ("rss_stat", "u64", "rss_stat")],
        "rss_stat": [("count", "u64", None), ("event", "u32", None)],
    },
    "v2": {
        # mm MOVED to offset 24; rss_stat.count RENAMED to rss_stat.stat
        "task_struct": [("state", "u64", None), ("flags", "u64", None),
                        ("pid", "u64", None), ("mm", "ptr", "mm_struct")],
        "mm_struct": [("mmap_base", "u64", None), ("hiwater_rss", "u64", None),
                      ("rss_stat", "u64", "rss_stat")],
        "rss_stat": [("stat", "u64", None)],
    },
}


def offsets(version):
    """Return {struct: {field: (offset, child_struct)}} for a version."""
    out = {}
    for struct, fields in LAYOUT[version].items():
        off = 0
        d = {}
        for name, typ, child in fields:
            d[name] = (off, child)
            off += SZ[typ]
        out[struct] = d
    return out


V1 = offsets("v1")
V2 = offsets("v2")

# region bases of each struct inside the 64-byte memory image
REGION = {"task_struct": 0, "mm_struct": 32, "rss_stat": 56}

# ground truth values per (version, struct, field)
TRUTH = {
    "v1": {"task.mm": 32, "task.pid": 0x1122, "mm.mmap_base": 0x60000000,
           "rss.count": 0x00C0FFEE, "rss.event": 7},
    "v2": {"task.mm": 32, "task.pid": 0x1122, "task.flags": 0xA11CE,
           "mm.mmap_base": 0x60000000, "rss.stat": 0x5A17},
}
FALLBACK = 0xF411BAC


def build_kernel(version):
    """Zeroed 72-byte image; each struct stored at its region base."""
    mem = bytearray(72)
    lay = offsets(version)

    def write(sname, field, value):
        foff, _ = lay[sname][field]
        struct.pack_into("<Q", mem, REGION[sname] + foff, value)

    write("task_struct", "mm", REGION["mm_struct"])
    write("task_struct", "pid", 0x1122)
    if "flags" in lay["task_struct"]:
        write("task_struct", "flags", 0xA11CE)
    write("mm_struct", "mmap_base", 0x60000000)
    write("mm_struct", "rss_stat", REGION["rss_stat"])
    if "count" in lay["rss_stat"]:
        write("rss_stat", "count", 0x00C0FFEE)
        write("rss_stat", "event", 7)
    if "stat" in lay["rss_stat"]:
        write("rss_stat", "stat", 0x5A17)
    return mem


def unpack(mem, addr):
    return struct.unpack_from("<Q", mem, addr)[0]


def resolve(path, version):
    """Return the chain offset of a field path, or None if a field is missing
    on this kernel. This is the existence/oracle check libbpf performs against
    vmlinux BTF at load time."""
    struct = "task_struct"
    off = 0
    for i, name in enumerate(path):
        if struct not in version or name not in version[struct]:
            return None
        foff, child = version[struct][name]
        off += foff
        if i == len(path) - 1:
            return off
        struct = child
    return None


def relocated_read(mem, version, path):
    """CO-RE style: resolve every step against the target version's layout,
    follow pointer fields by reading the stored region base."""
    struct = "task_struct"
    base = REGION["task_struct"]
    for i, name in enumerate(path):
        if struct not in version or name not in version[struct]:
            return None  # relocation cannot resolve -> libbpf load failure
        foff, child = version[struct][name]
        addr = base + foff
        if i == len(path) - 1:
            return unpack(mem, addr)
        base = unpack(mem, addr)
        struct = child
    return None


HARDCODED_MM = 16  # offsetof(task_struct, mm) hardcoded from v1's BTF


def field_at(version, struct, offset):
    for name, (foff, _child) in version[struct].items():
        if foff == offset:
            return name
    return "?"


def hardcoded_mm(mem, version):
    """Bad pattern: no relocation, the program was compiled for v1's offsets
    and is run unchanged on v2."""
    addr = REGION["task_struct"] + HARDCODED_MM
    return unpack(mem, addr)


def run():
    mem_v1 = build_kernel("v1")
    mem_v2 = build_kernel("v2")
    results = []  # (scenario, kernel, status) status in {"PASS", "REJECT_DETECTED"}

    print("CO-RE field-offset relocation model (guarded access degrades, "
          "hardcoded offsets break)\n")
    print(f"v1 layout: task_struct.mm@{V1['task_struct']['mm'][0]}, "
          f"rss_stat.count exists")
    print(f"v2 layout: task_struct.mm@{V2['task_struct']['mm'][0]} (moved), "
          f"rss_stat.count renamed to rss_stat.stat\n")

    # 1. relocated access to task.mm -- portable on both kernels
    for ver, mem, name in (("v1", mem_v1, "v1"), ("v2", mem_v2, "v2")):
        got = relocated_read(mem, {"v1": V1, "v2": V2}[ver], ("mm",))
        ok = got == TRUTH[ver]["task.mm"]
        results.append(("relocated task.mm", name, "PASS" if ok else "FAIL"))
        print(f"  [1] relocated read task.mm on {name}: got {got:#x}, "
              f"expected {TRUTH[ver]['task.mm']:#x} -> "
              f"{'PASS' if ok else 'FAIL'}")

    # 2. guarded access to optional field -- degrades gracefully on v2
    for ver, mem, name in (("v1", mem_v1, "v1"), ("v2", mem_v2, "v2")):

        def guarded():
            lay = {"v1": V1, "v2": V2}[ver]
            if resolve(("mm", "rss_stat", "count"), lay) is not None:
                return relocated_read(mem, lay, ("mm", "rss_stat", "count"))
            return FALLBACK

        got = guarded()
        expected = TRUTH[ver].get("rss.count", FALLBACK)
        ok = got == expected
        results.append(("guarded task.mm.rss_stat.count", name,
                        "PASS" if ok else "FAIL"))
        print(f"  [2] guarded read rss_stat.count on {name}: got {got:#x}, "
              f"expected {expected:#x} -> {'PASS' if ok else 'FAIL'}")

    # 3. UNGUARDED access to optional field -- relocation failure on v2
    for ver, mem, name in (("v1", mem_v1, "v1"), ("v2", mem_v2, "v2")):
        lay = {"v1": V1, "v2": V2}[ver]
        got = relocated_read(mem, lay, ("mm", "rss_stat", "count"))
        if got is None:
            status = "REJECT_DETECTED"  # relocation cannot resolve: load fails
            print(f"  [3] unguarded read rss_stat.count on {name}: unresolvable "
                  f"-> REJECT_DETECTED (load-time relocation failure)")
        else:
            status = "PASS" if got == TRUTH[ver].get("rss.count") else "FAIL"
            print(f"  [3] unguarded read rss_stat.count on {name}: reads "
                  f"{got:#x} -> {status}")
        results.append(("unguarded rss_stat.count", name, status))

    # 4. hardcoded offset -- wrong value silently on v2
    for ver, mem, name in (("v1", mem_v1, "v1"), ("v2", mem_v2, "v2")):
        got = hardcoded_mm(mem, {"v1": V1, "v2": V2}[ver])
        ok = got == TRUTH[ver]["task.mm"]
        if ok:
            note = "mm still at +0x10 on this kernel, read correct"
            status = "PASS"
        else:
            note = ("silently read task_struct.%s: mm moved to +0x18 on v2, "
                    "offset +0x10 now holds another field"
                    % field_at({"v1": V1, "v2": V2}[ver], "task_struct",
                               HARDCODED_MM))
            status = "REJECT_DETECTED"  # wrong value, kernel still loads it
        results.append(("hardcoded offsetof(task_struct,mm)", name, status))
        print(f"  [4] hardcoded offset +0x10 on {name}: got {got:#x} -> "
              f"{status if ok else 'REJECT_DETECTED: ' + note}")

    # summary
    print()
    passes = [r for r in results if r[2] == "PASS"]
    rejects = [r for r in results if r[2] == "REJECT_DETECTED"]
    print(f"RESULT: {len(passes)} portable accesses read correctly "
          f"({', '.join('%s@%s' % (p[0], p[1]) for p in passes)}); the checker "
          f"correctly REJECTS {len(rejects)} non-portable patterns on v2: "
          f"unguarded optional field -> load-time relocation failure, hardcoded "
          f"offset -> silent wrong value (correct on v1 only because it was "
          f"tuned to v1's offsets). A program using patterns [1] and [2] is "
          f"Compile-Once, Run-Everywhere; patterns [3] and [4] must be rejected.")
    return 0


if __name__ == "__main__":
    raise SystemExit(run())
