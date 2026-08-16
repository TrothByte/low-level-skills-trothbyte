# GOOD: a 5-stage accelerator pipeline (LayerNorm-style) with correct sync
# placement. Every cross-unit write-read pair on a shared buffer has a barrier
# strictly between write and read (barrier sufficiency, AccelSync model,
# arxiv-2605-07881). The checker reports SAFE with 0 hazards.
# Runs with plain python 3.11.

PROG = [
    ("insn", "DMA", "write", "buf0"),   # 0  DMA load in -> buf0
    ("bar",),                            # 1  covers DMA->VEC on buf0
    ("insn", "VEC", "read", "buf0"),    # 2  reduce
    ("insn", "VEC", "write", "buf1"),   # 3
    ("bar",),                            # 4  covers VEC->SCL on buf1
    ("insn", "SCL", "read", "buf1"),    # 5  mean/var stats
    ("insn", "SCL", "write", "buf2"),   # 6
    ("bar",),                            # 7  covers SCL->VEC on buf2
    ("insn", "VEC", "read", "buf2"),    # 8  normalize
    ("insn", "VEC", "read", "buf0"),    # 9  normalize reads original too
    ("insn", "VEC", "write", "buf3"),   # 10
    ("bar",),                            # 11 covers VEC->MAT on buf3
    ("insn", "MAT", "read", "buf3"),    # 12 scale
    ("insn", "MAT", "write", "buf4"),   # 13
    ("bar",),                            # 14 covers MAT->DMA on buf4
    ("insn", "DMA", "read", "buf4"),    # 15 store out
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
                    f"read@{j}({u2}) -- no barrier between; a stale-read "
                    f"interleaving EXISTS (golden run can pass by luck)")
    for h in hazards:
        print(h)
    if not hazards:
        print("  no hazards: every cross-unit write-read pair on the same")
        print("  buffer has a barrier strictly between write and read.")
        print("  verdict: SAFE (barrier-sufficient)")
    else:
        print(f"  verdict: UNSAFE ({len(hazards)} hazard(s))")
    print()
    return not hazards


if __name__ == "__main__":
    print("accelerator pipeline sync checker (barrier sufficiency)\n")
    assert analyze(PROG, "LayerNorm pipeline, barriers after every producer")
    print("RESULT: correct barrier placement -> SAFE. A golden run and the")
    print("static check agree; but only the static check is a correctness")
    print("argument (the golden run samples one interleaving).")
