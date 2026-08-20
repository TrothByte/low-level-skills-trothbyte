#!/usr/bin/env python3
"""BAD: python model of the wrong approach -- a "portable" eBPF program whose
author hardcoded struct offsets from one kernel's disassembly/BTF instead of
recording relocations. The good checker (examples/good/core_reloc_model.py)
must reject this program. This file plays the role of that checker running on
the bad program's access strategy and prints the diagnostics a load would
produce on kernels v1 and v2.

Plain python 3.11, no dependencies. Run:

    python examples/bad/reloc_misuse.py
"""

import struct

SZ = {"u64": 8, "u32": 4, "ptr": 8}

# Same two kernel layouts as the good model: v2 moves task_struct.mm from
# offset 16 to 24 and renames rss_stat.count to rss_stat.stat.
LAYOUT = {
    "v1": {
        "task_struct": [("state", "u64", None), ("pid", "u64", None), ("mm", "ptr", "mm_struct")],
        "mm_struct": [("mmap_base", "u64", None), ("rss_stat", "u64", "rss_stat")],
        "rss_stat": [("count", "u64", None), ("event", "u32", None)],
    },
    "v2": {
        "task_struct": [("state", "u64", None), ("flags", "u64", None),
                        ("pid", "u64", None), ("mm", "ptr", "mm_struct")],
        "mm_struct": [("mmap_base", "u64", None), ("hiwater_rss", "u64", None),
                      ("rss_stat", "u64", "rss_stat")],
        "rss_stat": [("stat", "u64", None)],
    },
}


def offsets(version):
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

REGION = {"task_struct": 0, "mm_struct": 32, "rss_stat": 56}
TRUTH = {
    "v1": {"task.mm": 32, "task.pid": 0x1122, "mm.mmap_base": 0x60000000,
           "rss.count": 0x00C0FFEE, "rss.event": 7},
    "v2": {"task.mm": 32, "task.pid": 0x1122, "task.flags": 0xA11CE,
           "mm.mmap_base": 0x60000000, "rss.stat": 0x5A17},
}

# "offsets captured from the v1 kernel" -- the author's hardcoded constants.
HARD_MM = 16            # offsetof(task_struct, mm) on v1
HARD_RSS = 8            # offsetof(mm_struct, rss_stat) on v1
HARD_COUNT = 0          # offsetof(rss_stat, count) on v1


def build_kernel(version):
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
    if "stat" in lay["rss_stat"]:
        write("rss_stat", "stat", 0x5A17)
    return mem


def unpack(mem, addr):
    return struct.unpack_from("<Q", mem, addr)[0]


def field_at(version, struct, offset):
    lay = {"v1": V1, "v2": V2}[version]
    for name, (foff, _child) in lay[struct].items():
        if foff == offset:
            return name
    return "?"


def bad_read_mm(mem, version):
    """The bad program's access: `*(struct mm_struct **)(task + HARD_MM)`.
    No relocation: the offset is a compile-time constant from kernel v1."""
    value = unpack(mem, REGION["task_struct"] + HARD_MM)
    correct = value == TRUTH[version]["task.mm"]
    what = field_at(version, "task_struct", HARD_MM)
    return value, correct, what


def bad_read_count(mem, version):
    """The bad program's optional-field read WITHOUT bpf_core_field_exists:
    the relocation for rss_stat.count cannot resolve on v2."""
    lay = {"v1": V1, "v2": V2}[version]
    # simulate libbpf relocation failure for a missing field
    if "count" not in lay["rss_stat"]:
        return None
    base = unpack(mem, REGION["task_struct"] + HARD_MM)
    mm_base = base if base == REGION["mm_struct"] else None
    if mm_base is None:
        return None
    rss_addr = unpack(mem, mm_base + HARD_RSS)
    return unpack(mem, rss_addr + HARD_COUNT)


def run():
    mem_v1 = build_kernel("v1")
    mem_v2 = build_kernel("v2")

    print("BAD program model: hardcoded offsets from kernel v1, no CO-RE "
          "relocation, no bpf_core_field_exists guards\n")

    rejected = 0

    # access 1: hardcoded task_struct.mm
    for ver, mem in (("v1", mem_v1), ("v2", mem_v2)):
        value, correct, what = bad_read_mm(mem, ver)
        if correct:
            print(f"  {ver}: task+0x10 -> mm pointer {value:#x} (offset 0x10 "
                  f"is task_struct.mm on v1, so this read is correct)")
        else:
            print(f"  {ver}: task+0x10 -> {value:#x} == task_struct.{what} "
                  f"-- NOT the mm pointer (expects "
                  f"{TRUTH[ver]['task.mm']:#x}). The kernel loads the program "
                  f"and it silently reads the wrong field.")
            rejected += 1

    # access 2: unguarded optional field rss_stat.count
    for ver, mem in (("v1", mem_v1), ("v2", mem_v2)):
        count = bad_read_count(mem, ver)
        if count is not None:
            print(f"  {ver}: rss_stat.count read -> {count:#x}")
        else:
            print(f"  {ver}: relocation for rss_stat.count cannot resolve "
                  f"(field renamed to rss_stat.stat on v2). libbpf load error: "
                  f"'can't find field 'rss_stat.count'' -- program fails to "
                  f"load at runtime on v2, or worse, the field is read "
                  f"uninitialized if the fallback is missing.")
            rejected += 1

    print()
    print(f"RESULT: checker REJECTS this program: {rejected}/2 accesses are "
          f"wrong or unloadable on kernel v2. The good checker "
          f"(examples/good/core_reloc_model.py) flags hardcoded offsets and "
          f"unguarded optional fields; portable code uses bpf_core_read + "
          f"bpf_core_field_exists and records relocations, not offsets.")
    return 1  # rejected, like bpftool prog load exiting non-zero


if __name__ == "__main__":
    raise SystemExit(run())
