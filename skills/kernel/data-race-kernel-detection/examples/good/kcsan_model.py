# GOOD: a faithful model of the KCSAN detection criterion.
# Data race (LKMM): two accesses conflict (same location, >=1 write),
# happen concurrently, and at least one is a plain access.
# Marked accesses are annotated but never set up watchpoints, so a
# marked-only conflict is NOT reported. The model also implements the
# ASSERT_EXCLUSIVE_* second pass for exclusivity logic.
# Run: python examples/good/kcsan_model.py   (expect exit 0)

PLAIN = "plain"
MARKED = "marked"

class Access:
    def __init__(self, loc, kind, is_write, thread, pc):
        self.loc, self.kind, self.is_write = loc, kind, is_write
        self.thread, self.pc = thread, pc

def detect(a, b):
    """KCSAN criterion: returns a report if a and b form a data race."""
    if a.loc != b.loc:
        return None
    if a.thread == b.thread:
        return None
    if not (a.is_write or b.is_write):
        return None
    if a.kind == MARKED and b.kind == MARKED:
        return None                     # not a data race (marked/marked)
    return f"BUG: KCSAN: data-race in {a.pc} / {b.pc}"

def exclusivity_pass(accesses, writers, targets):
    """ASSERT_EXCLUSIVE_WRITER-like check over the same schedule."""
    for w in writers:
        if w.loc not in targets:
            continue
        for other in accesses:
            if other is w or other.thread == w.thread:
                continue
            if other.is_write:
                return f"ASSERT_EXCLUSIVE_WRITER violated by {other.pc}"

def main():
    # schedule: two threads touch g_flag (plain-plain conflict -> race)
    t1 = [Access("g_flag", PLAIN, True, 1, "writer+0x1d")]
    t2 = [Access("g_flag", PLAIN, False, 2, "reader+0x10")]
    races = [detect(a, b) for a in t1 for b in t2 if detect(a, b)]
    assert len(races) == 1 and "data-race" in races[0], races
    print(f"case1 (plain-plain): {races[0]}")

    # marked/marked conflict: NOT a data race, but exclusivity violation
    m1 = Access("g_state", MARKED, True, 1, "writer_a+0x0")
    m2 = Access("g_state", MARKED, True, 2, "writer_b+0x0")
    assert detect(m1, m2) is None
    viol = exclusivity_pass([m1, m2], [m1, m2], {"g_state"})
    print(f"case2 (marked/marked): no data race, but {viol}")

    print("GOOD: detection criterion and exclusivity pass behave per KCSAN")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
