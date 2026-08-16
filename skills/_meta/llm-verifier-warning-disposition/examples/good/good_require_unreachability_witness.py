# GOOD: warning disposition with the burden of proof on dismissal. A no-bug
# decision requires a witness: either a reachability walker trace proving the
# error state is unreachable from every entry, or a solver/verifier pass.
# Based on arxiv-2606-15122 (Evident: dismissal requires establishing
# unreachability, not plausibility) and arxiv-2605-21434 (agents propose,
# solvers verify). Runs with plain python 3.11.


def analyze_fn(fn, cap):
    """Walk one entry function. Tracks the range of 'n'; returns
    (overflow_reachable, trace_lines)."""
    lo, hi = 0, 0
    trace = []
    for stmt in fn:
        kind = stmt[0]
        if kind == "read":
            lo, hi = 0, float("inf")
            trace.append(f"    {stmt[1]} = read()        -> n in [0, inf)")
        elif kind == "guard_max":
            limit = stmt[1]
            hi = min(hi, limit)
            trace.append(f"    guard n <= {limit}      -> n in [0, {limit}]")
        elif kind == "copy":
            if hi > cap:
                trace.append(f"    copy(buf, src, n)    -> n.hi={hi:.0f} > cap={cap}: "
                             "OVERFLOW REACHABLE")
                return True, trace
            trace.append(f"    copy(buf, src, n)    -> n.hi={hi:.0f} <= cap={cap}: safe")
    return False, trace


ENTRIES = {
    "parse_http":   [("read", "n"), ("guard_max", 16), ("copy", 32)],
    "parse_async":  [("read", "n"), ("copy", 32)],           # 2nd entry, unguarded
    "parse_trusted": [("read", "n"), ("guard_max", 8), ("copy", 32)],
}


def disposition(entries, cap):
    """Return (verdicts) with witnesses. DISMISS only with a walker trace that
    proves unreachability from every entry; RETAIN with the reachable entry's
    trace as witness."""
    reachable = []
    witnesses = {}
    for name, fn in entries.items():
        overflow, trace = analyze_fn(fn, cap)
        witnesses[name] = trace
        if overflow:
            reachable.append(name)
    return reachable, witnesses


def report(reachable, witnesses, cap):
    print(f"analyzing overflow-at-copy warnings (buffer cap {cap})")
    for name, trace in witnesses.items():
        print(f"  entry {name}:")
        for ln in trace:
            print(f"    {ln}")
    if reachable:
        print(f"  -> RETAIN  (error state reachable via: {', '.join(reachable)})")
        print(f"     witness: {', '.join(reachable)} entry traces above")
        return "RETAIN"
    print("  -> DISMISS (error state unreachable from every entry)")
    print("     witness: walker traces above show n.hi <= cap on all paths")
    return "DISMISS"


if __name__ == "__main__":
    print("warning disposition with reachability witnesses\n")

    reachable, witnesses = disposition(ENTRIES, cap=32)
    assert report(reachable, witnesses, 32) == "RETAIN"

    # a genuinely unreachable warning (only parse_trusted exists) may be
    # dismissed -- but only with the walker trace as the witness
    print()
    reachable2, witnesses2 = disposition({"parse_trusted": ENTRIES["parse_trusted"]}, cap=32)
    assert report(reachable2, witnesses2, 32) == "DISMISS"

    print("\nRESULT: reachable warning retained WITH witness; provably")
    print("unreachable warning dismissed WITH witness. No prose-only verdicts.")
