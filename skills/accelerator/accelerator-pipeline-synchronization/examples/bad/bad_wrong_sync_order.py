# BAD: a pipeline with a barrier that is present but MISPLACED. The first
# barrier sits AFTER the vector read it was meant to guard (index 2 > read at
# index 1), so the DMA->VEC pair on buf0 stays unordered. The second barrier
# sits between the VEC write and the SCL read but is positioned before the SCL
# read while the pair (write@3 -> read@4) needs it strictly between -- it is
# outside the interval because the read follows it directly with no coverage
# between write and read. Barrier existence is not barrier coverage.
# # intentionally incorrect
#
# Arxiv-2605-07881: "misplaced synchronization primitive". Runs with python 3.11.

PROG = [
    ("insn", "DMA", "write", "buf0"),   # 0  DMA load in -> buf0
    ("insn", "VEC", "read", "buf0"),    # 1  reduce -- reads before the barrier
    ("bar",),                            # 2  barrier AFTER the read: useless for (0,1)
    ("insn", "VEC", "write", "buf1"),   # 3
    ("insn", "SCL", "read", "buf1"),    # 4  stats -- no barrier between (3,4)
    ("bar",),                            # 5  barrier AFTER the read too
    ("insn", "SCL", "write", "buf2"),   # 6
    ("bar",),                            # 7
    ("insn", "VEC", "read", "buf2"),    # 8
    ("insn", "VEC", "write", "buf3"),   # 9
    ("bar",),                            # 10
    ("insn", "MAT", "read", "buf3"),    # 11
    ("insn", "MAT", "write", "buf4"),   # 12
    ("bar",),                            # 13
    ("insn", "DMA", "read", "buf4"),    # 14
]


def analyze(prog, name):
    print(f"program: {name}")
    events, bars = [], set()
    for i, item in enumerate(prog):
        if item[0] == "bar":
            bars.add(i)
        else:
            _, unit, op, buf = item
            events.append((i, unit, op, buf))

    hazards = []
    for (i, u1, op1, b1) in events:
        if op1 != "write":
            continue
        for (j, u2, op2, b2) in events:
            if op2 != "read" or i == j or b2 != b1:
                continue
            if u1 == u2:
                if i > j:
                    hazards.append(
                        f"  HAZARD same-unit {b1}: read@{j} before write@{i}")
                continue
            if not any(i < b < j for b in bars):
                hazards.append(
                    f"  HAZARD cross-unit {b1}: write@{i}({u1}) -> "
                    f"read@{j}({u2}) -- barriers exist but none sits strictly "
                    f"between write and read; stale-read interleaving EXISTS")
    for h in hazards:
        print(h)
    if not hazards:
        print("  no hazards found.")
        print("  verdict: SAFE (barrier-sufficient)")
    else:
        print(f"  verdict: UNSAFE ({len(hazards)} hazard(s))")
    print()
    return not hazards


if __name__ == "__main__":
    print("accelerator pipeline sync checker (barrier sufficiency)\n")
    assert not analyze(PROG, "LayerNorm pipeline, barriers MISPLACED")
    print("RESULT: barriers exist in the program but none covers the")
    print("DMA->VEC and VEC->SCL pairs. 'There is a barrier' is not a verdict;")
    print("coverage is.")
