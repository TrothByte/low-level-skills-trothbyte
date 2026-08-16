# BAD: self-certified dismissal. The same agent that proposed the bug
# hypothesis also "certifies" that it is a false positive ("I have checked all
# paths"). There is no separate verification step and no witness. The
# reachability ground truth still finds the path, so the certification is
# unsound -- this is the "agents propose, solvers verify" violation from
# arxiv-2605-21434.
# # intentionally incorrect

ENTRIES = {
    "parse_http":  [("read", "n"), ("guard_max", 16), ("copy", 32)],
    "parse_async": [("read", "n"), ("copy", 32)],
}


def agent_certifies_dismissal(warning):
    print(f"  warning {warning}:")
    print("    hypothesis: 'possible overflow at copy(buf, src, n)'")
    print("    certification: 'after careful review of all code paths, this")
    print("                    warning is a false positive' -> DISMISS")
    print("    (certificate is a self-attestation; no walker/solver output)")
    return "DISMISS"


def reachability_check():
    for name, fn in ENTRIES.items():
        hi = float("inf")
        for stmt in fn:
            if stmt[0] == "guard_max":
                hi = min(hi, stmt[1])
            elif stmt[0] == "copy":
                if hi > stmt[1]:
                    print(f"  ground truth: entry {name!r} reaches copy with "
                          f"n.hi={hi:.0f} > cap={stmt[1]} -> OVERFLOW REACHABLE")
                    return True
    return False


if __name__ == "__main__":
    print("self-certified dismissal (proposer verifies itself)\n")
    verdict = agent_certifies_dismissal("overflow at copy(buf, src, n)")
    reachable = reachability_check()
    print(f"\n  agent verdict: {verdict}")
    if reachable and verdict == "DISMISS":
        print("  >>> UNSOUND: the proposing agent certified its own dismissal.")
        print("      The reachability check shows the error state is reachable.")
        print("      Separation of roles is the requirement, not a preference.")
