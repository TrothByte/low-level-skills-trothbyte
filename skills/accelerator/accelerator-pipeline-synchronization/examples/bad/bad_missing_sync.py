# BAD: a 5-stage accelerator pipeline with the two stage-boundary barriers
# omitted. DMA->VEC (buf0) and VEC->SCL (buf1) are cross-unit write-read
# pairs with NO barrier between. The race is hardware-visible and escapes
# simulation and golden testing: a golden run can schedule the DMA write
# first and pass. The static coverage check catches it.
# # intentionally incorrect
#
# Arxiv-2605-07881 (AccelSync): "missing or misplaced synchronization
# primitive introduces hardware-visible data races that escape both
# simulation and golden testing". Runs with plain python 3.11.

PROG = [
    ("insn", "DMA", "write", "buf0"),   # 0  DMA load in -> buf0
    # barrier MISSING here (DMA -> VEC on buf0 is unordered)
    ("insn", "VEC", "read", "buf0"),    # 1  reduce -- may read stale data
    ("insn", "VEC", "write", "buf1"),   # 2
    # barrier MISSING here (VEC -> SCL on buf1 is unordered)
    ("insn", "SCL", "read", "buf1"),    # 3  stats -- may read stale data
    ("insn", "SCL", "write", "buf2"),   # 4
    ("bar",),                            # 5
    ("insn", "VEC", "read", "buf2"),    # 6
    ("insn", "VEC", "write", "buf3"),   # 7
    ("bar",),                            # 8
    ("insn", "MAT", "read", "buf3"),    # 9
    ("insn", "MAT", "write", "buf4"),   # 10
    ("bar",),                            # 11
    ("insn", "DMA", "read", "buf4"),    # 12 store out
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
        print("  no hazards found.")
        print("  verdict: SAFE (barrier-sufficient)")
    else:
        print(f"  verdict: UNSAFE ({len(hazards)} hazard(s))")
    print()
    return not hazards


if __name__ == "__main__":
    print("accelerator pipeline sync checker (barrier sufficiency)\n")
    assert not analyze(PROG, "LayerNorm pipeline, stage barriers MISSING")
    print("RESULT: two cross-unit hazards (DMA->VEC on buf0, VEC->SCL on")
    print("buf1). A golden run that happened to schedule the DMA first would")
    print("pass -- which is exactly why golden testing is not sufficient.")
